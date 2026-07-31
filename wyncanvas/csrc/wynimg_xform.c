// wynimg_xform - geometric transforms over LINEAR PREMULTIPLIED float32 RGBA.
//
// WHY PREMULTIPLIED DATA IS WHAT MAKES A FILTERED RESIZE CORRECT.
//
// Every function here that is not an exact integer permutation computes a
// WEIGHTED AVERAGE of neighbouring pixels. That is the same operation as the
// blur in csrc/wynimg_filter.c, and it is correct for exactly the same reason:
// a weighted average of PREMULTIPLIED samples averages "colour contribution"
// and "coverage" with the same weights, so a fully transparent neighbour
// contributes nothing at all. Unpremultiply first and you are averaging bare
// colour, and the colour of a transparent pixel is UNDEFINED - stored as 0,
// i.e. black - so every soft edge in the image drags black inward and comes
// back with a DARK HALO. Worked example: scaling a 2x1 buffer of
// [opaque white, fully transparent] up by 2 must give a half-covered pixel that
// is still WHITE at 50% coverage (stored 0.5,0.5,0.5,0.5). Averaging
// unpremultiplied colour gives colour 0.5 (grey) at alpha 0.5, which stores as
// 0.25 - a white edge that has gone grey purely because it faded out.
//
// Alpha is filtered with the identical kernel, which is what keeps the result a
// representable premultiplied pixel.
//
// ONE PASS, NOT TWO. wynimg_xf_affine takes a full 2x3 matrix so that
// scale + rotate + translate composes into a SINGLE resampling pass. Doing it as
// scale-then-rotate resamples the image twice: the second pass filters values
// that are already filtered, so the effective kernel is the CONVOLUTION of the
// two kernels and the result is visibly softer than the one-pass answer. (Two
// bilinear passes at 45 degrees is a well-known way to turn a crisp edge into a
// two-pixel ramp.) Compose the matrices, not the images.
//
// EXACT PATHS EXIST ON PURPOSE. A flip, a 180 turn and a 90/270 turn are index
// permutations: every output pixel IS some input pixel. Running them through the
// general resampler would filter data that needs no filtering, and at 90 degrees
// cos(pi/2) is 6.1e-17 rather than 0, so the taps miss the pixel centres by a
// hair and the result is a fractionally blurred image that is no longer
// reversible. wynimg_xf_flip and wynimg_xf_rot90 memcpy 4 floats per pixel, so
// a 90 / -90 round trip is BIT-IDENTICAL. tests/test_xform.wyn asserts equality
// (not approximity) precisely to prove the exact path was taken.
//
// FFI CONTRACT, as everywhere in this shim: Wyn `int` <-> C `long long`,
// Wyn `float` <-> C `double`. Handles arrive as `void*` and are resolved through
// wynimg_deref(), so a stale handle is a safe no-op and never a use-after-free.
// Every function returns a NEW handle (or 0), never mutating its input - see the
// header note in src/xform.wyn for why that is what makes a transform undoable.
#include "wynimg.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// Prototypes live here rather than in csrc/wynimg.h: this translation unit is
// self-contained (nothing else in the shim calls into it), and the declarations
// that matter across the boundary are the ones in src/xform.wyn.

// Resampling kernels. The numbering is part of the FFI contract with
// src/xform.wyn.
#define XF_NEAREST  0
#define XF_BILINEAR 1
#define XF_BICUBIC  2

// Edge behaviour for the general (affine / rotate) path. Numbering is part of
// the FFI contract.
#define XF_EDGE_TRANSPARENT 0
#define XF_EDGE_CLAMP       1

// Anchors for wynimg_xf_canvas, row-major: 0 TL 1 TC 2 TR / 3 ML 4 C 5 MR /
// 6 BL 7 BC 8 BR. Part of the FFI contract.
#define XF_ANCHOR_COUNT 9

void* wynimg_xf_scale(void* srcp, long long ow, long long oh,
                      long long kern, void* maskp);
void* wynimg_xf_affine(void* srcp, double a, double b, double c, double d,
                       double e, double f, long long ow, long long oh,
                       long long kern, long long edge, void* maskp);
void* wynimg_xf_rotate(void* srcp, double degrees,
                       long long kern, long long edge, void* maskp);
void* wynimg_xf_flip(void* srcp, long long axis);
void* wynimg_xf_rot90(void* srcp, long long quarters);
void* wynimg_xf_crop(void* srcp, long long x, long long y,
                     long long w, long long h);
void* wynimg_xf_canvas(void* srcp, long long w, long long h, long long anchor);
double    wynimg_xf_alpha_sum(void* p);
long long wynimg_xf_kernel_count(void);

// ---------------------------------------------------------------------------
// Kernels.
//
// Each k_*() is the continuous filter evaluated at a distance in PIXELS, and
// each has a support radius. They are unit-integral in the sense that matters
// here: the tap weights are normalised by their sum before use (see
// weights_build), so a truncated kernel cannot darken the image the way an
// unnormalised gaussian darkens a blur.
// ---------------------------------------------------------------------------

static double k_tent(double t) {
    t = fabs(t);
    return t < 1.0 ? 1.0 - t : 0.0;
}

// Catmull-Rom: the Keys cubic with a = -0.5. Chosen over a B-spline because it
// INTERPOLATES (k(0)=1, k(+-1)=0, k(+-2)=0), so a 1:1 resample is the identity
// and an integer-aligned tap reproduces the source pixel exactly. A B-spline
// blurs even at 1:1.
//
// It has negative lobes, so it OVERSHOOTS at a hard step - the classic
// Catmull-Rom artefact (halo/ringing). See the CLAMP note at store_px.
static double k_catrom(double t) {
    t = fabs(t);
    if (t < 1.0) return ((1.5 * t - 2.5) * t) * t + 1.0;
    if (t < 2.0) return ((-0.5 * t + 2.5) * t - 4.0) * t + 2.0;
    return 0.0;
}

static double kern_radius(long long kern) {
    if (kern == XF_BICUBIC) return 2.0;
    if (kern == XF_BILINEAR) return 1.0;
    return 0.5;                             // nearest; never actually filtered
}

static double kern_eval(long long kern, double t) {
    if (kern == XF_BICUBIC) return k_catrom(t);
    return k_tent(t);                       // bilinear
}

static long long clampi(long long v, long long lo, long long hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// THE CLAMP, and it is a decision rather than defensive noise.
//
// Catmull-Rom's negative lobes let a resampled value land outside the range of
// its inputs. Left alone that produces a NEGATIVE alpha and colour > alpha,
// neither of which is a representable premultiplied pixel: it misbehaves the
// instant it is composited (a negative alpha makes the backdrop brighter than
// it started) and it round-trips through PNG as garbage.
//
// So the result is clamped to the valid premultiplied domain: alpha into [0,1]
// and each colour into [0, alpha]. This kills the OUT-OF-GAMUT half of the
// ringing and leaves the in-gamut half, which is the part users actually want
// from a bicubic (it is the acutance). tests/test_xform.wyn asserts a hard step
// upscaled bicubically stays inside [0,1]; removing this clamp turns it red.
static void store_px(float* dst, double r, double g, double b, double a) {
    if (a < 0.0) a = 0.0;
    if (a > 1.0) a = 1.0;
    if (r < 0.0) r = 0.0;
    if (r > a)   r = a;
    if (g < 0.0) g = 0.0;
    if (g > a)   g = a;
    if (b < 0.0) b = 0.0;
    if (b > a)   b = a;
    dst[0] = (float)r;
    dst[1] = (float)g;
    dst[2] = (float)b;
    dst[3] = (float)a;
}

// ---------------------------------------------------------------------------
// Coverage-mask gate.
//
// A transform can be restricted to a selection exactly the way a filter is, and
// through the same representation: a float coverage buffer whose R channel is
// linear coverage (see csrc/wynimg_select.c). The mask is in OUTPUT space,
// because that is where the geometry of the result lives - a transform moves
// pixels, so "the selected region" can only mean a region of the answer.
//
// The gate is a multiply of ALL FOUR channels by coverage, which for
// premultiplied data is exactly "this pixel is only partly there". Filters blend
// original*(1-cov) + filtered*cov; here the destination is a brand-new
// transparent buffer, so the original term is 0 and the blend collapses to the
// multiply.
//
// A mask of the wrong size is REFUSED (the whole call returns 0) rather than
// ignored: silently transforming the whole layer when the caller asked for a
// selection is the failure mode that loses a user's work.
// ---------------------------------------------------------------------------
static int mask_apply(WynImg* out, void* maskp) {
    if (!maskp) return 1;                   // 0 means "the whole buffer"
    WynImg* m = wynimg_deref(maskp);
    if (!m || !m->px) return 0;
    if (m->w != out->w || m->h != out->h) return 0;
    long long n = out->w * out->h;
    for (long long i = 0; i < n; i++) {
        float cov = m->px[i * 4 + 0];
        if (cov < 0.0f) cov = 0.0f;
        if (cov > 1.0f) cov = 1.0f;
        out->px[i * 4 + 0] *= cov;
        out->px[i * 4 + 1] *= cov;
        out->px[i * 4 + 2] *= cov;
        out->px[i * 4 + 3] *= cov;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// SCALE - separable, with a footprint that widens when minifying.
//
// TWO PASSES HERE ARE NOT "RESAMPLING TWICE". A separable filter applied along
// x and then along y IS the 2D tensor-product filter, exactly - not an
// approximation of it. The softening argument at the top of this file is about
// two SEPARATE geometric operations (scale then rotate), each with its own
// resampling; this is one operation factored into O(r) work per axis instead of
// O(r^2).
//
// ANTIALIASING ON DOWNSCALE: the filter footprint is scaled by 1/scale when
// scale < 1, so a 4x reduction averages over 4 source pixels per output pixel
// per axis rather than taking one tap. For the tent kernel that IS a box-ish
// prefilter (a 4-wide triangle); for Catmull-Rom it is a widened Catmull-Rom.
// This is the standard "widen the reconstruction filter to double as a
// prefilter" scheme, chosen over building a mip pyramid because a pyramid only
// has power-of-two levels and still needs this for the fractional remainder.
//
// Without it, a downscale takes one tap per output pixel: a 1px checkerboard is
// sampled at a fixed phase and comes back SOLID - the entire signal aliased away.
// Measured at 3x (48 -> 16): widened gives 0.494..0.506, unwidened gives
// 1.0 / 0.0. And a 1-in-16 dot grid reduced 4x comes back 0.25 instead of the
// correct 0.0625, i.e. FOUR TIMES too bright.
//
// A WARNING FOR WHOEVER TESTS THIS NEXT: an EVEN reduction factor does not
// detect the missing widening at all. At 4x the tent's centre lands on a
// half-integer source position, so even the unwidened filter takes two equal
// taps and returns 0.5 by phase luck - deleting the widening leaves a 4x
// checkerboard test GREEN. tests/test_xform.wyn therefore uses an ODD factor for
// the checkerboard and a total-coverage assertion for the even case.
//
// NEAREST DELIBERATELY DOES NOT WIDEN. "Nearest" is the mode a user picks to
// keep pixel-art crisp and to see the true sample values; a nearest that
// averaged would be a bilinear with extra steps. It is therefore implemented as
// direct index rounding (not through the weight machinery), which also
// guarantees the exact replication a 2x nearest upscale must produce.
//
// EDGES CLAMP. A scale has no content outside the source, and zero-padding a
// PREMULTIPLIED buffer means "surrounded by transparency", so it would eat the
// frame of an opaque image - the same argument the blur makes in
// csrc/wynimg_filter.c. Transparent-border is only meaningful for a transform
// that rotates content away from the edge, so it is a parameter of
// affine/rotate, not of scale.
// ---------------------------------------------------------------------------

typedef struct {
    long long  taps;    // taps per output sample (fixed, zero-padded)
    long long* first;   // osize entries: index of the first tap
    double*    w;       // osize * taps weights, already normalised
} XfWeights;

static void weights_free(XfWeights* wt) {
    free(wt->first);
    free(wt->w);
    wt->first = NULL;
    wt->w = NULL;
}

static int weights_build(XfWeights* wt, long long osize, long long isize,
                         long long kern) {
    double scale  = (double)osize / (double)isize;
    double fscale = scale < 1.0 ? 1.0 / scale : 1.0;
    double radius = kern_radius(kern) * fscale;
    long long taps = (long long)ceil(radius * 2.0) + 2;

    wt->taps  = taps;
    wt->first = (long long*)malloc((size_t)osize * sizeof(long long));
    wt->w     = (double*)malloc((size_t)osize * (size_t)taps * sizeof(double));
    if (!wt->first || !wt->w) { weights_free(wt); return 0; }

    for (long long i = 0; i < osize; i++) {
        // Centre of output pixel i in SOURCE INDEX space (0.0 = centre of
        // source pixel 0). The two half-pixels are what make a 1:1 scale the
        // identity and a 2x nearest upscale replicate exactly; dropping them is
        // the classic half-pixel shift.
        double si    = ((double)i + 0.5) / scale - 0.5;
        long long f0 = (long long)ceil(si - radius);
        wt->first[i] = f0;
        double sum = 0.0;
        for (long long t = 0; t < taps; t++) {
            double wv = kern_eval(kern, (si - (double)(f0 + t)) / fscale);
            wt->w[i * taps + t] = wv;
            sum += wv;
        }
        if (sum != 0.0 && sum != 1.0) {
            for (long long t = 0; t < taps; t++) wt->w[i * taps + t] /= sum;
        }
    }
    return 1;
}

// Point-sampled resize. Separate from the filtered path so that "nearest" is
// provably a copy of source pixels and nothing else.
static void* scale_nearest(const WynImg* s, long long ow, long long oh) {
    void* outp = wynimg_new(ow, oh);
    WynImg* o = wynimg_deref(outp);
    if (!o || !o->px) { wynimg_free(outp); return NULL; }
    double sx = (double)ow / (double)s->w;
    double sy = (double)oh / (double)s->h;
    for (long long y = 0; y < oh; y++) {
        long long iy = (long long)floor(((double)y + 0.5) / sy);
        iy = clampi(iy, 0, s->h - 1);
        for (long long x = 0; x < ow; x++) {
            long long ix = (long long)floor(((double)x + 0.5) / sx);
            ix = clampi(ix, 0, s->w - 1);
            memcpy(&o->px[(y * ow + x) * 4], &s->px[(iy * s->w + ix) * 4],
                   4 * sizeof(float));
        }
    }
    return outp;
}

void* wynimg_xf_scale(void* srcp, long long ow, long long oh,
                      long long kern, void* maskp) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;
    if (ow <= 0 || oh <= 0) return NULL;

    void* outp;
    if (kern == XF_NEAREST) {
        outp = scale_nearest(s, ow, oh);
        if (!outp) return NULL;
    } else {
        XfWeights wx = {0, NULL, NULL};
        XfWeights wy = {0, NULL, NULL};
        if (!weights_build(&wx, ow, s->w, kern)) return NULL;
        if (!weights_build(&wy, oh, s->h, kern)) { weights_free(&wx); return NULL; }

        // Horizontal pass into an ow x ih intermediate, then vertical into the
        // result. Allocated through wynimg_new so it inherits the same overflow
        // and size guards as every other buffer here.
        void* tmpp = wynimg_new(ow, s->h);
        outp = wynimg_new(ow, oh);
        WynImg* t = wynimg_deref(tmpp);
        WynImg* o = wynimg_deref(outp);
        if (!t || !t->px || !o || !o->px) {
            wynimg_free(tmpp); wynimg_free(outp);
            weights_free(&wx); weights_free(&wy);
            return NULL;
        }
        for (long long y = 0; y < s->h; y++) {
            for (long long x = 0; x < ow; x++) {
                double acc[4] = {0.0, 0.0, 0.0, 0.0};
                for (long long k = 0; k < wx.taps; k++) {
                    double wv = wx.w[x * wx.taps + k];
                    if (wv == 0.0) continue;
                    long long ix = clampi(wx.first[x] + k, 0, s->w - 1);
                    const float* p = &s->px[(y * s->w + ix) * 4];
                    acc[0] += wv * p[0]; acc[1] += wv * p[1];
                    acc[2] += wv * p[2]; acc[3] += wv * p[3];
                }
                store_px(&t->px[(y * ow + x) * 4], acc[0], acc[1], acc[2], acc[3]);
            }
        }
        for (long long y = 0; y < oh; y++) {
            for (long long x = 0; x < ow; x++) {
                double acc[4] = {0.0, 0.0, 0.0, 0.0};
                for (long long k = 0; k < wy.taps; k++) {
                    double wv = wy.w[y * wy.taps + k];
                    if (wv == 0.0) continue;
                    long long iy = clampi(wy.first[y] + k, 0, s->h - 1);
                    const float* p = &t->px[(iy * ow + x) * 4];
                    acc[0] += wv * p[0]; acc[1] += wv * p[1];
                    acc[2] += wv * p[2]; acc[3] += wv * p[3];
                }
                store_px(&o->px[(y * ow + x) * 4], acc[0], acc[1], acc[2], acc[3]);
            }
        }
        wynimg_free(tmpp);
        weights_free(&wx);
        weights_free(&wy);
    }

    WynImg* o = wynimg_deref(outp);
    if (!mask_apply(o, maskp)) { wynimg_free(outp); return NULL; }
    return outp;
}

// ---------------------------------------------------------------------------
// The general sampler, used by affine and therefore by rotate.
//
// (cx, cy) are CONTINUOUS source coordinates with the centre of pixel (0,0) at
// (0.5, 0.5) - the same convention the bounding-box maths below uses, so the
// two cannot drift apart.
//
// EDGE MODES. XF_EDGE_CLAMP clamps tap indices to the border, which extends the
// edge colour outward; the weights still sum to 1, so an opaque image keeps its
// frame opaque. XF_EDGE_TRANSPARENT lets out-of-range taps contribute ZERO and
// deliberately does NOT renormalise: a tap outside the image is fully
// transparent, contributing nothing to either colour or coverage, so alpha falls
// off across the boundary and the rotated shape gets an antialiased edge for
// free. Renormalising there would smear the border colour outward into a
// hard-edged fringe instead.
// ---------------------------------------------------------------------------
static void sample_at(const WynImg* s, double cx, double cy,
                      long long kern, long long edge, float* dst) {
    double fx = cx - 0.5;                   // index space
    double fy = cy - 0.5;

    if (kern == XF_NEAREST) {
        long long ix = (long long)floor(fx + 0.5);
        long long iy = (long long)floor(fy + 0.5);
        if (ix < 0 || iy < 0 || ix >= s->w || iy >= s->h) {
            if (edge != XF_EDGE_CLAMP) {
                dst[0] = dst[1] = dst[2] = dst[3] = 0.0f;
                return;
            }
            ix = clampi(ix, 0, s->w - 1);
            iy = clampi(iy, 0, s->h - 1);
        }
        // A copy, not an average: nearest must reproduce source values bit for
        // bit or "nearest" is a lie.
        memcpy(dst, &s->px[(iy * s->w + ix) * 4], 4 * sizeof(float));
        return;
    }

    double r = kern_radius(kern);
    long long x0 = (long long)ceil(fx - r);
    long long y0 = (long long)ceil(fy - r);
    long long n  = (long long)(2.0 * r);     // 2 taps for tent, 4 for catrom
    double acc[4] = {0.0, 0.0, 0.0, 0.0};

    for (long long j = 0; j < n; j++) {
        long long iy = y0 + j;
        double wy = kern_eval(kern, fy - (double)iy);
        if (wy == 0.0) continue;
        long long sy = iy;
        if (sy < 0 || sy >= s->h) {
            if (edge != XF_EDGE_CLAMP) continue;
            sy = clampi(sy, 0, s->h - 1);
        }
        for (long long i = 0; i < n; i++) {
            long long ix = x0 + i;
            double wv = wy * kern_eval(kern, fx - (double)ix);
            if (wv == 0.0) continue;
            long long sxi = ix;
            if (sxi < 0 || sxi >= s->w) {
                if (edge != XF_EDGE_CLAMP) continue;
                sxi = clampi(sxi, 0, s->w - 1);
            }
            const float* p = &s->px[(sy * s->w + sxi) * 4];
            acc[0] += wv * p[0]; acc[1] += wv * p[1];
            acc[2] += wv * p[2]; acc[3] += wv * p[3];
        }
    }
    store_px(dst, acc[0], acc[1], acc[2], acc[3]);
}

// ---------------------------------------------------------------------------
// AFFINE - the one-pass composition point.
//
// The matrix maps SOURCE continuous coordinates to DESTINATION continuous
// coordinates:
//     dx = a*x + b*y + e
//     dy = c*x + d*y + f
// It is inverted here and the OUTPUT is walked, sampling the source - the only
// way to fill every output pixel exactly once (a forward scatter leaves holes
// wherever the map expands).
//
// AUTO BOUNDING BOX: pass ow <= 0 or oh <= 0 and the output is sized to the
// bounding box of the transformed source rectangle and translated so that box
// starts at (0,0). That is what a rotate wants (nothing is cropped off), and it
// is computed from the same corner maths for every transform rather than being
// special-cased per operation.
//
// A SINGULAR MATRIX IS REFUSED (returns 0). A zero determinant collapses the
// image to a line; there is no inverse to sample with, and returning an empty
// buffer would look like a working transform that lost the layer.
//
// LIMITATION, stated rather than hidden: the sampler uses a UNIT footprint, so a
// heavy MINIFICATION expressed as an affine aliases the way an unprefiltered
// downscale does. Anisotropic footprints (EWA) are not implemented. Downscale
// with wynimg_xf_scale first, then affine the rest - which is also the only
// combination where two passes are justified.
// ---------------------------------------------------------------------------
void* wynimg_xf_affine(void* srcp, double a, double b, double c, double d,
                       double e, double f, long long ow, long long oh,
                       long long kern, long long edge, void* maskp) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;

    double det = a * d - b * c;
    if (fabs(det) < 1e-12) return NULL;

    double sw = (double)s->w;
    double sh = (double)s->h;
    double xs[4], ys[4];
    xs[0] = e;                       ys[0] = f;
    xs[1] = a * sw + e;              ys[1] = c * sw + f;
    xs[2] = b * sh + e;              ys[2] = d * sh + f;
    xs[3] = a * sw + b * sh + e;     ys[3] = c * sw + d * sh + f;
    double minx = xs[0], maxx = xs[0], miny = ys[0], maxy = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < minx) minx = xs[i];
        if (xs[i] > maxx) maxx = xs[i];
        if (ys[i] < miny) miny = ys[i];
        if (ys[i] > maxy) maxy = ys[i];
    }

    double tx = e, ty = f;
    if (ow <= 0 || oh <= 0) {
        ow = (long long)ceil(maxx - minx - 1e-9);
        oh = (long long)ceil(maxy - miny - 1e-9);
        if (ow < 1) ow = 1;
        if (oh < 1) oh = 1;
        tx = e - minx;
        ty = f - miny;
    }
    if (ow > WYNIMG_MAX_PIXELS || oh > WYNIMG_MAX_PIXELS) return NULL;

    double ia =  d / det, ib = -b / det;
    double ic = -c / det, id =  a / det;

    void* outp = wynimg_new(ow, oh);
    WynImg* o = wynimg_deref(outp);
    if (!o || !o->px) { wynimg_free(outp); return NULL; }

    for (long long y = 0; y < oh; y++) {
        double dy = (double)y + 0.5 - ty;
        for (long long x = 0; x < ow; x++) {
            double dx = (double)x + 0.5 - tx;
            double cx = ia * dx + ib * dy;
            double cy = ic * dx + id * dy;
            sample_at(s, cx, cy, kern, edge, &o->px[(y * ow + x) * 4]);
        }
    }
    if (!mask_apply(o, maskp)) { wynimg_free(outp); return NULL; }
    return outp;
}

// ROTATE about the source centre, by an arbitrary angle, with a correct output
// bounding box (nothing is cropped).
//
// POSITIVE DEGREES ROTATE CLOCKWISE ON SCREEN. Image y grows downward, so the
// textbook matrix [[cos,-sin],[sin,cos]] appears clockwise; picking the other
// sign to look "mathematical" is how a UI ends up with a rotate button that
// turns the wrong way.
//
// This is a thin wrapper over wynimg_xf_affine on purpose: one bounding-box
// implementation and one sampling loop, so rotate cannot disagree with affine.
// Right angles should go to wynimg_xf_rot90 instead - see the note at the top.
void* wynimg_xf_rotate(void* srcp, double degrees,
                       long long kern, long long edge, void* maskp) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;
    double th = degrees * (M_PI / 180.0);
    double co = cos(th), si = sin(th);
    double cx = (double)s->w * 0.5;
    double cy = (double)s->h * 0.5;
    // dest = R*(src - centre) + centre  =>  translation = centre - R*centre.
    double e = cx - (co * cx - si * cy);
    double f = cy - (si * cx + co * cy);
    return wynimg_xf_affine(srcp, co, -si, si, co, e, f, 0, 0, kern, edge, maskp);
}

// ---------------------------------------------------------------------------
// EXACT INTEGER PATHS. No arithmetic touches a pixel value in any of these -
// they permute or copy whole 4-float pixels, so they are lossless and exactly
// invertible. No mask parameter, deliberately: gating by coverage would multiply
// pixel values and destroy the bit-exactness that is the whole point.
// ---------------------------------------------------------------------------

// axis 0 = horizontal (mirror left/right), anything else = vertical.
void* wynimg_xf_flip(void* srcp, long long axis) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;
    void* outp = wynimg_new(s->w, s->h);
    WynImg* o = wynimg_deref(outp);
    if (!o || !o->px) { wynimg_free(outp); return NULL; }
    for (long long y = 0; y < s->h; y++) {
        for (long long x = 0; x < s->w; x++) {
            long long dx = (axis == 0) ? (s->w - 1 - x) : x;
            long long dy = (axis == 0) ? y : (s->h - 1 - y);
            memcpy(&o->px[(dy * s->w + dx) * 4], &s->px[(y * s->w + x) * 4],
                   4 * sizeof(float));
        }
    }
    return outp;
}

// quarters: 1 = 90 clockwise, 2 = 180, 3 = 270 clockwise (= 90 anticlockwise).
// Any integer is accepted and reduced mod 4, so -1 is 270 and a UI can just
// add or subtract. 0 is an exact clone, which keeps "rotate by nothing" a
// no-op that still returns a NEW buffer like every other function here.
void* wynimg_xf_rot90(void* srcp, long long quarters) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;
    long long q = quarters % 4;
    if (q < 0) q += 4;

    long long ow = (q == 1 || q == 3) ? s->h : s->w;
    long long oh = (q == 1 || q == 3) ? s->w : s->h;
    void* outp = wynimg_new(ow, oh);
    WynImg* o = wynimg_deref(outp);
    if (!o || !o->px) { wynimg_free(outp); return NULL; }

    for (long long y = 0; y < s->h; y++) {
        for (long long x = 0; x < s->w; x++) {
            long long dx, dy;
            if (q == 0)      { dx = x;                dy = y; }
            else if (q == 1) { dx = s->h - 1 - y;     dy = x; }
            else if (q == 2) { dx = s->w - 1 - x;     dy = s->h - 1 - y; }
            else             { dx = y;                dy = s->w - 1 - x; }
            memcpy(&o->px[(dy * ow + dx) * 4], &s->px[(y * s->w + x) * 4],
                   4 * sizeof(float));
        }
    }
    return outp;
}

// CROP discards: the result is exactly w x h taken from (x,y) of the source.
//
// A rect that hangs off the edge is NOT an error - the parts with no source are
// TRANSPARENT (the buffer starts zeroed and those pixels are simply not
// written). Refusing instead would make "crop to the marquee" fail whenever a
// marquee was dragged past the border, which is most of the time.
void* wynimg_xf_crop(void* srcp, long long x, long long y,
                     long long w, long long h) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;
    if (w <= 0 || h <= 0) return NULL;
    void* outp = wynimg_new(w, h);
    WynImg* o = wynimg_deref(outp);
    if (!o || !o->px) { wynimg_free(outp); return NULL; }
    for (long long j = 0; j < h; j++) {
        long long sy = y + j;
        if (sy < 0 || sy >= s->h) continue;
        for (long long i = 0; i < w; i++) {
            long long sx = x + i;
            if (sx < 0 || sx >= s->w) continue;
            memcpy(&o->px[(j * w + i) * 4], &s->px[(sy * s->w + sx) * 4],
                   4 * sizeof(float));
        }
    }
    return outp;
}

// CANVAS RESIZE pads. The difference from crop is not cosmetic: crop CHOOSES a
// rectangle of the image, canvas-resize changes the document size and puts the
// existing pixels somewhere inside it, so growing the canvas adds TRANSPARENCY
// rather than picture. The new area must be transparent and not black - in a
// premultiplied buffer those two look identical in RGB (both store 0,0,0) and
// differ only in alpha, which is why tests/test_xform.wyn asserts alpha 0 AND
// colour 0.
//
// `anchor` says where the old content sits: 0 TL 1 TC 2 TR / 3 ML 4 C 5 MR /
// 6 BL 7 BC 8 BR. Centring uses integer division, so an odd leftover goes to the
// right/bottom - stated because "centred" has two answers and a test needs one.
//
// Shrinking with an anchor is well defined and does the obvious thing (the
// offset goes negative and the content is cropped), so this is also the general
// "reframe the document" operation.
void* wynimg_xf_canvas(void* srcp, long long w, long long h, long long anchor) {
    WynImg* s = wynimg_deref(srcp);
    if (!s || !s->px) return NULL;
    if (w <= 0 || h <= 0) return NULL;
    if (anchor < 0 || anchor >= XF_ANCHOR_COUNT) return NULL;

    long long col = anchor % 3;             // 0 left, 1 centre, 2 right
    long long row = anchor / 3;             // 0 top,  1 middle, 2 bottom
    long long ox = (col == 0) ? 0 : (col == 1 ? (w - s->w) / 2 : w - s->w);
    long long oy = (row == 0) ? 0 : (row == 1 ? (h - s->h) / 2 : h - s->h);

    // A pad is a crop at the negated offset. One implementation, so pad and
    // crop cannot disagree about which pixel goes where.
    return wynimg_xf_crop(srcp, -ox, -oy, w, h);
}

// Total stored alpha. Exists so a test can assert that a rotation CONSERVES
// COVERAGE - nothing vanished and nothing was invented - which is the one
// property of a resampled rotation that no single-pixel assertion can express.
// -1.0 when refused; a real sum of clamped alpha is never negative.
double wynimg_xf_alpha_sum(void* p) {
    WynImg* s = wynimg_deref(p);
    if (!s || !s->px) return -1.0;
    double sum = 0.0;
    long long n = s->w * s->h;
    for (long long i = 0; i < n; i++) sum += s->px[i * 4 + 3];
    return sum;
}

long long wynimg_xf_kernel_count(void) { return 3; }

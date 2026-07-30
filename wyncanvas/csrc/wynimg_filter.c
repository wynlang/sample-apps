// wynimg_filter - image adjustments and convolutions over LINEAR PREMULTIPLIED
// float32 RGBA.
//
// THE ONE RULE THAT DECIDES EVERY FUNCTION IN THIS FILE.
//
// Buffers hold colour ALREADY MULTIPLIED BY ALPHA. That makes two classes of
// operation behave completely differently, and mixing them up is the classic
// image-editor bug:
//
//   * A CONVOLUTION (blur) must run on the PREMULTIPLIED data. This is not a
//     shortcut, it is the correct thing: a weighted average of premultiplied
//     samples averages "colour contribution" and "coverage" with the same
//     weights, so a transparent neighbour contributes nothing rather than
//     contributing black. Unpremultiplying first and blurring the bare colour
//     drags the (undefined, usually 0) colour of transparent pixels into the
//     result and gives every soft edge a DARK HALO. Alpha is convolved with the
//     same kernel, which is what keeps the result a valid premultiplied pixel.
//
//   * A POINT OPERATION that is not linear-and-homogeneous -- a per-channel
//     CURVE, levels, contrast, brightness, invert, threshold, posterize -- must
//     UNPREMULTIPLY, apply, then REPREMULTIPLY. Every one of those has either a
//     constant term or a non-linear shape, and f(a*c) != a*f(c) for those, so
//     applying them to stored values shifts the COLOUR of a semi-transparent
//     pixel. Worked example, and it is not subtle: a pixel that is 50%-opaque
//     mid-grey stores 0.107. `contrast(2.0)` around the mid-grey pivot 0.214
//     must leave it exactly on the pivot; applied to the stored 0.107 it lands
//     on 0.0 -- a mid-grey turned black purely because it was half transparent.
//
//   * Saturation and hue rotation ARE linear, so premultiplication commutes with
//     them in exact arithmetic -- but only until the CLAMP. Boosting saturation
//     on the stored values clamps in the wrong scale and can leave colour > alpha,
//     which is not a representable premultiplied pixel at all and blows up the
//     moment it is composited. They unpremultiply here too, so the clamp happens
//     where the gamut actually is.
//
// GAUSSIAN BLUR IS SEPARABLE. Two 1D passes, O(r) per pixel, not O(r^2). At
// radius 30 that is 61 taps instead of 3721 -- a 61x difference, which is the
// difference between a usable slider and a frozen editor.
//
// BOX BLUR IS A RUNNING SUM, so its cost per pixel is independent of radius.
// That is what makes it the right primitive for a live preview.
//
// BORDERS CLAMP TO EDGE, THEY DO NOT ZERO-PAD. Zero-padding a premultiplied
// buffer means "the image is surrounded by transparency", so blurring an opaque
// photo eats its frame: the corner of a uniform white field comes back grey and
// half-transparent. Clamping replicates the edge sample, which keeps the kernel
// weights summing to 1 over real data and leaves a uniform field bit-identical.
//
// RESTRICTED APPLICATION. Every entry point takes a coverage-mask handle; 0
// means the whole buffer. The mask's R channel is linear coverage, exactly as
// wynimg_composite and the paint/text coverage buffers use it, and the result is
// `orig*(1-cov) + filtered*cov`. That is the whole mechanism a selection needs,
// and putting it in the driver rather than in each filter means a new filter
// cannot forget it.
//
// FFI CONTRACT: every boundary function takes handles as `void*` (Wyn `int` ->
// C `long long` -> pointer-width value) and resolves them through
// wynimg_deref(), so a stale handle is a null-path no-op and never a
// use-after-free. Scalars are `double` / `long long` only.
#include "wynimg.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Small helpers. Deliberately duplicated from the other shim files rather than
// exported from one: they are three lines each, and a shared header of static
// inlines would be a build dependency for no benefit.
// ---------------------------------------------------------------------------

static double clamp01d(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

// A NaN already sitting in a buffer must be normalised, not propagated: one NaN
// channel otherwise poisons every pixel a convolution touches.
static double denan(double v) { return (v != v) ? 0.0 : v; }

static long long climp(long long v, long long lo, long long hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// Rec.709 luminance (ITU-R BT.709 / sRGB primaries), the weights that belong
// with this project's primaries. 0.2126 / 0.7152 / 0.0722 -- NOT 1/3 each, and
// not Rec.601's 0.299/0.587/0.114. An equal-weight "greyscale" turns pure red
// into a 0.333 grey when its actual luminance is 0.2126: 57% too bright, and
// obviously wrong the first time a red is compared with a green.
#define WYN_LUM_R 0.2126
#define WYN_LUM_G 0.7152
#define WYN_LUM_B 0.0722

static double lum709(double r, double g, double b) {
    return WYN_LUM_R * r + WYN_LUM_G * g + WYN_LUM_B * b;
}

// The LINEAR value of sRGB 0.5, i.e. perceptual mid-grey.
//
// WHICH PIVOT CONTRAST USES, AND WHY IT IS THIS ONE. "Contrast" means "push
// values away from the middle", and the middle a user means is the middle of the
// grey they see, which is sRGB 0.5. In LINEAR light that is 0.2140, not 0.5 --
// linear 0.5 is sRGB 0.735, a light grey. Pivoting at linear 0.5 makes a
// contrast boost darken everything below three-quarters brightness, which reads
// as "contrast also made it much darker". So the pivot is the linear value of
// sRGB 0.5, computed once here rather than spelled as a magic number:
//   ((0.5 + 0.055) / 1.055) ^ 2.4 = 0.2140411
// Levels' gamma and the curve LUT, by contrast, are documented as operating on
// LINEAR values directly -- they are explicit numeric controls, not a "make it
// punchier" slider.
#define WYN_MID_GREY_LINEAR 0.21404114

// ---------------------------------------------------------------------------
// Mask resolution.
//
// A stale mask handle resolves to NULL and is treated as "no mask" (whole
// buffer), matching wynimg_composite. A mask whose dimensions disagree with the
// target is REFUSED, because silently ignoring it would apply a filter to the
// whole image when the user asked for a selection.
// ---------------------------------------------------------------------------

typedef enum { MASK_NONE, MASK_OK, MASK_BAD } MaskState;

static MaskState mask_resolve(const WynImg* target, void* maskp, const WynImg** out) {
    *out = NULL;
    if (!maskp) return MASK_NONE;
    WynImg* m = wynimg_deref(maskp);
    if (!m || !m->px) return MASK_NONE;
    if (m->w != target->w || m->h != target->h) return MASK_BAD;
    *out = m;
    return MASK_OK;
}

static double mask_cov(const WynImg* m, size_t o) {
    if (!m) return 1.0;
    return clamp01d(denan(m->px[o + 0]));
}

// ---------------------------------------------------------------------------
// Point-operation driver.
//
// Every non-convolution filter is one callback over an UNPREMULTIPLIED RGB
// triple. Centralising the unpremultiply / clamp / repremultiply / coverage
// blend here is the point: a filter added later physically cannot forget the
// premultiply dance or the mask, because it never sees a stored value.
// ---------------------------------------------------------------------------

// One parameter block for all of them. Unused members are ignored; this is
// cheaper to read than fourteen one-field structs.
typedef struct {
    double a, b, c;
    long long n;
    const WynImg* lut;
} FParam;

typedef void (*PointFn)(double* rgb, const FParam* p);

static long long apply_point(void* bufp, void* maskp, PointFn fn, const FParam* p) {
    WynImg* im = wynimg_deref(bufp);
    if (!im || !im->px) return 0;
    const WynImg* m = NULL;
    if (mask_resolve(im, maskp, &m) == MASK_BAD) return 0;

    long long npx = im->w * im->h;
    for (long long i = 0; i < npx; i++) {
        size_t o = (size_t)i * 4;
        double cov = mask_cov(m, o);
        if (cov <= 0.0) continue;                 // outside the selection

        double al = denan(im->px[o + 3]);
        al = clamp01d(al);
        if (al <= 0.0) continue;                  // fully transparent: no colour to adjust

        // UNPREMULTIPLY. See the file header: this is what stops a
        // semi-transparent pixel shifting colour under a non-linear curve.
        double rgb[3];
        for (int ch = 0; ch < 3; ch++) rgb[ch] = clamp01d(denan(im->px[o + ch]) / al);

        double orig[3];
        orig[0] = rgb[0]; orig[1] = rgb[1]; orig[2] = rgb[2];

        fn(rgb, p);

        for (int ch = 0; ch < 3; ch++) {
            // Clamp in UNPREMULTIPLIED space, where the gamut actually is, then
            // re-premultiply. Clamping after the multiply is how a saturation
            // boost ends up storing colour > alpha.
            double v = clamp01d(rgb[ch]);
            double mixed = orig[ch] * (1.0 - cov) + v * cov;
            im->px[o + ch] = (float)(mixed);
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Convolution driver.
//
// Convolutions produce a whole new plane, so unlike the point ops they need a
// scratch buffer; the coverage blend then happens once, plane against plane.
// Scratch is plain malloc rather than wynimg_new: these are private, they must
// not consume handle-table slots, and they must be freed on every early return.
// ---------------------------------------------------------------------------

static float* plane_alloc(long long w, long long h) {
    // w and h come from a live WynImg, so wynimg_new's WYNIMG_MAX_PIXELS guard
    // has already ruled out an overflowing product.
    return (float*)malloc((size_t)(w * h * 4) * sizeof(float));
}

// out = orig*(1-cov) + filt*cov, in premultiplied space, and then the
// premultiplied invariant colour <= alpha is restored. A blur cannot violate it
// (it is a convex combination of valid pixels) but unsharp mask can, and doing
// the repair here means every convolution-shaped filter gets it.
static void blend_plane(WynImg* im, const float* filt, const WynImg* m) {
    long long npx = im->w * im->h;
    for (long long i = 0; i < npx; i++) {
        size_t o = (size_t)i * 4;
        double cov = mask_cov(m, o);
        if (cov <= 0.0) continue;
        double al = clamp01d(denan(im->px[o + 3]) * (1.0 - cov)
                             + denan(filt[o + 3]) * cov);
        for (int ch = 0; ch < 3; ch++) {
            double v = denan(im->px[o + ch]) * (1.0 - cov) + denan(filt[o + ch]) * cov;
            if (v < 0.0) v = 0.0;
            if (v > al) v = al;                   // keep colour <= alpha
            im->px[o + ch] = (float)v;
        }
        im->px[o + 3] = (float)al;
    }
}

// ---- gaussian --------------------------------------------------------------

// Cap on the kernel half-width. 512 is far past any sane slider and keeps the
// kernel allocation trivially small; a caller asking for more is refused rather
// than given a multi-minute convolution.
#define WYN_BLUR_MAX_RADIUS 512.0

// Builds a normalised 1D gaussian of half-width r.
//
// sigma = radius/3 so the kernel is truncated at 3 sigma, which contains 99.73%
// of the distribution -- truncating closer than that leaves a visible step at
// the kernel edge.
//
// THE NORMALISATION IS LOAD-BEARING, not tidiness. The discrete samples of a
// gaussian do not sum to 1 (they sum to ~0.9973 at 3 sigma, and to much less for
// small sigma where the tails are cut hard), so an unnormalised kernel makes a
// blur DARKEN and de-opacify everything it touches. tests/test_filter.wyn's
// "blur of a uniform field is unchanged" is exactly this check.
static double* gauss_kernel(double radius, long long* out_r) {
    double sigma = radius / 3.0;
    if (sigma < 1e-6) sigma = 1e-6;
    long long r = (long long)ceil(radius);
    if (r < 1) r = 1;
    double* k = (double*)malloc((size_t)(2 * r + 1) * sizeof(double));
    if (!k) return NULL;
    double sum = 0.0;
    for (long long t = -r; t <= r; t++) {
        double e = exp(-((double)t * (double)t) / (2.0 * sigma * sigma));
        k[t + r] = e;
        sum += e;
    }
    for (long long t = 0; t < 2 * r + 1; t++) k[t] /= sum;
    *out_r = r;
    return k;
}

// One separable pass. `stride` is the element step along the blur axis and
// `count` the number of samples along it; running the same code for both axes is
// what guarantees the two passes cannot disagree.
static void gauss_pass(const float* src, float* dst,
                       long long w, long long h,
                       const double* k, long long r, int vertical) {
    long long along  = vertical ? h : w;
    long long across = vertical ? w : h;
    long long step   = vertical ? w * 4 : 4;
    long long base_step = vertical ? 4 : w * 4;

    for (long long j = 0; j < across; j++) {
        const float* sline = src + (size_t)(j * base_step);
        float*       dline = dst + (size_t)(j * base_step);
        for (long long i = 0; i < along; i++) {
            double acc[4] = { 0.0, 0.0, 0.0, 0.0 };
            for (long long t = -r; t <= r; t++) {
                // CLAMP TO EDGE: out-of-range taps read the nearest real
                // sample, so the weights still sum to 1 over real data and the
                // border does not darken.
                long long s = climp(i + t, 0, along - 1);
                const float* sp = sline + (size_t)(s * step);
                double wgt = k[t + r];
                for (int ch = 0; ch < 4; ch++) acc[ch] += wgt * denan(sp[ch]);
            }
            float* dp = dline + (size_t)(i * step);
            for (int ch = 0; ch < 4; ch++) dp[ch] = (float)acc[ch];
        }
    }
}

// Writes the gaussian-blurred PREMULTIPLIED plane of `im` into `out`.
// 1 on success, 0 on allocation failure.
static int gauss_into(const WynImg* im, float* out, double radius) {
    long long r = 0;
    double* k = gauss_kernel(radius, &r);
    if (!k) return 0;
    float* tmp = plane_alloc(im->w, im->h);
    if (!tmp) { free(k); return 0; }
    gauss_pass(im->px, tmp, im->w, im->h, k, r, 0);   // horizontal
    gauss_pass(tmp, out, im->w, im->h, k, r, 1);      // vertical
    free(tmp);
    free(k);
    return 1;
}

// Gaussian blur, separable, clamp-to-edge. `radius` is the kernel half-width in
// pixels (sigma = radius/3). radius <= 0 is a successful no-op, which is what a
// slider at zero should be. Returns 1/0.
long long wynimg_flt_gaussian(void* bufp, double radius, void* maskp) {
    WynImg* im = wynimg_deref(bufp);
    if (!im || !im->px) return 0;
    if (radius != radius) return 0;                        // NaN refused, not absorbed
    if (radius > WYN_BLUR_MAX_RADIUS) return 0;
    const WynImg* m = NULL;
    if (mask_resolve(im, maskp, &m) == MASK_BAD) return 0;
    if (radius <= 0.0) return 1;

    float* filt = plane_alloc(im->w, im->h);
    if (!filt) return 0;
    if (!gauss_into(im, filt, radius)) { free(filt); return 0; }
    blend_plane(im, filt, m);
    free(filt);
    return 1;
}

// ---- box -------------------------------------------------------------------

// One separable box pass by RUNNING SUM: the window is built once per line and
// then slid one sample at a time, so the per-pixel cost is two adds regardless
// of radius. Clamp-to-edge again, applied to both the entering and the leaving
// index, which is what keeps the window width constant at 2r+1 everywhere and
// therefore keeps a uniform field exactly unchanged.
static void box_pass(const float* src, float* dst,
                     long long w, long long h,
                     long long r, int vertical) {
    long long along  = vertical ? h : w;
    long long across = vertical ? w : h;
    long long step   = vertical ? w * 4 : 4;
    long long base_step = vertical ? 4 : w * 4;
    double inv = 1.0 / (double)(2 * r + 1);

    for (long long j = 0; j < across; j++) {
        const float* sline = src + (size_t)(j * base_step);
        float*       dline = dst + (size_t)(j * base_step);
        for (int ch = 0; ch < 4; ch++) {
            double sum = 0.0;
            for (long long t = -r; t <= r; t++)
                sum += denan(sline[(size_t)(climp(t, 0, along - 1) * step) + ch]);
            dline[ch] = (float)(sum * inv);
            for (long long i = 1; i < along; i++) {
                sum += denan(sline[(size_t)(climp(i + r, 0, along - 1) * step) + ch]);
                sum -= denan(sline[(size_t)(climp(i - r - 1, 0, along - 1) * step) + ch]);
                dline[(size_t)(i * step) + ch] = (float)(sum * inv);
            }
        }
    }
}

// Box blur. `radius` is in pixels; the window is 2*radius+1. radius <= 0 is a
// successful no-op. Returns 1/0.
long long wynimg_flt_box(void* bufp, long long radius, void* maskp) {
    WynImg* im = wynimg_deref(bufp);
    if (!im || !im->px) return 0;
    if ((double)radius > WYN_BLUR_MAX_RADIUS) return 0;
    const WynImg* m = NULL;
    if (mask_resolve(im, maskp, &m) == MASK_BAD) return 0;
    if (radius <= 0) return 1;

    float* tmp  = plane_alloc(im->w, im->h);
    float* filt = plane_alloc(im->w, im->h);
    if (!tmp || !filt) { free(tmp); free(filt); return 0; }
    box_pass(im->px, tmp, im->w, im->h, radius, 0);
    box_pass(tmp, filt, im->w, im->h, radius, 1);
    blend_plane(im, filt, m);
    free(tmp);
    free(filt);
    return 1;
}

// ---- unsharp mask ----------------------------------------------------------

// Sharpen = original + amount * (original - blurred).
//
// BUILT ON THE BLUR, not on a 3x3 kernel. A fixed 3x3 sharpen has one hard-coded
// scale and produces ringing on anything larger; unsharp masking with a radius
// is the operation photographers actually mean, and reusing gauss_into means the
// two cannot drift apart -- if the blur's normalisation broke, "sharpen of a
// uniform field is unchanged" fails too, which is exactly the coupling the tests
// assert.
//
// THRESHOLD IS WHY THIS IS USABLE ON A PHOTOGRAPH. Without it, unsharp masking
// amplifies sensor noise in flat areas (sky, skin) just as hard as it amplifies
// edges, because noise is precisely high-frequency detail. A pixel whose largest
// per-channel deviation from its blurred self is below `threshold` is left
// alone, so flat-but-noisy regions stay flat and only real edges are boosted.
//
// Operates on PREMULTIPLIED data, like the blur it is made of -- the difference
// of two premultiplied planes is what "local detail" means here, and
// unpremultiplying the difference would divide detail by a coverage that the
// difference has already accounted for.
long long wynimg_flt_unsharp(void* bufp, double radius, double amount,
                             double threshold, void* maskp) {
    WynImg* im = wynimg_deref(bufp);
    if (!im || !im->px) return 0;
    if (radius != radius || amount != amount || threshold != threshold) return 0;
    if (radius > WYN_BLUR_MAX_RADIUS) return 0;
    const WynImg* m = NULL;
    if (mask_resolve(im, maskp, &m) == MASK_BAD) return 0;
    if (radius <= 0.0 || amount == 0.0) return 1;
    if (threshold < 0.0) threshold = 0.0;

    float* blur = plane_alloc(im->w, im->h);
    if (!blur) return 0;
    if (!gauss_into(im, blur, radius)) { free(blur); return 0; }

    long long npx = im->w * im->h;
    for (long long i = 0; i < npx; i++) {
        size_t o = (size_t)i * 4;
        double d[4], mx = 0.0;
        for (int ch = 0; ch < 4; ch++) {
            d[ch] = denan(im->px[o + ch]) - denan(blur[o + ch]);
        }
        for (int ch = 0; ch < 3; ch++) {
            double ad = d[ch] < 0.0 ? -d[ch] : d[ch];
            if (ad > mx) mx = ad;
        }
        if (mx < threshold) {
            // Below the noise floor: reuse the ORIGINAL, so the plane handed to
            // blend_plane is "sharpened where it should be, untouched elsewhere"
            // and the mask blend below stays a single uniform step.
            for (int ch = 0; ch < 4; ch++) blur[o + ch] = im->px[o + ch];
            continue;
        }
        double al = clamp01d(denan(im->px[o + 3]) + amount * d[3]);
        blur[o + 3] = (float)al;
        for (int ch = 0; ch < 3; ch++) {
            double v = denan(im->px[o + ch]) + amount * d[ch];
            if (v < 0.0) v = 0.0;
            if (v > al) v = al;
            blur[o + ch] = (float)v;
        }
    }
    blend_plane(im, blur, m);
    free(blur);
    return 1;
}

// ---------------------------------------------------------------------------
// Point operations. Each is a two-line callback plus a boundary function; the
// unpremultiply, the clamp, the repremultiply and the mask blend all live in
// apply_point.
// ---------------------------------------------------------------------------

// Brightness: an ADDITIVE offset in linear light. Additive (not multiplicative)
// because that is what lifts shadows; the multiplicative control is `exposure`.
static void pf_brightness(double* rgb, const FParam* p) {
    for (int ch = 0; ch < 3; ch++) rgb[ch] += p->a;
}

long long wynimg_flt_brightness(void* bufp, double amount, void* maskp) {
    if (amount != amount) return 0;
    FParam p = { amount, 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_brightness, &p);
}

// Contrast: scale away from the perceptual mid-grey pivot. amount 1.0 is
// identity, 0.0 flattens to the pivot, >1 increases. See WYN_MID_GREY_LINEAR for
// why the pivot is 0.2140 and not 0.5.
static void pf_contrast(double* rgb, const FParam* p) {
    for (int ch = 0; ch < 3; ch++)
        rgb[ch] = WYN_MID_GREY_LINEAR + (rgb[ch] - WYN_MID_GREY_LINEAR) * p->a;
}

long long wynimg_flt_contrast(void* bufp, double amount, void* maskp) {
    if (amount != amount) return 0;
    if (amount < 0.0) return 0;
    FParam p = { amount, 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_contrast, &p);
}

// Exposure: multiply by 2^stops. THIS is the operation that is physically a
// change of exposure, and it is only meaningful in linear light -- the same
// multiply applied to sRGB-encoded values is not a stop of anything. One stop up
// doubles the linear value.
static void pf_exposure(double* rgb, const FParam* p) {
    for (int ch = 0; ch < 3; ch++) rgb[ch] *= p->a;
}

long long wynimg_flt_exposure(void* bufp, double stops, void* maskp) {
    if (stops != stops) return 0;
    if (stops < -32.0 || stops > 32.0) return 0;
    FParam p = { pow(2.0, stops), 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_exposure, &p);
}

// Saturation: interpolate against the pixel's own Rec.709 luminance. 0 is fully
// desaturated, 1 identity, >1 boosted.
static void pf_saturation(double* rgb, const FParam* p) {
    double y = lum709(rgb[0], rgb[1], rgb[2]);
    for (int ch = 0; ch < 3; ch++) rgb[ch] = y + (rgb[ch] - y) * p->a;
}

long long wynimg_flt_saturation(void* bufp, double amount, void* maskp) {
    if (amount != amount) return 0;
    if (amount < 0.0) return 0;
    FParam p = { amount, 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_saturation, &p);
}

// Greyscale: mix towards Rec.709 luminance. amount 1.0 is fully grey.
//
// Exposed separately from saturation(0) even though they coincide, because
// "greyscale" is the operation people look for and because the WEIGHTS are the
// whole content of this function: pure red must become 0.2126, not 0.3333.
static void pf_greyscale(double* rgb, const FParam* p) {
    double y = lum709(rgb[0], rgb[1], rgb[2]);
    for (int ch = 0; ch < 3; ch++) rgb[ch] = rgb[ch] + (y - rgb[ch]) * p->a;
}

long long wynimg_flt_greyscale(void* bufp, double amount, void* maskp) {
    if (amount != amount) return 0;
    FParam p = { clamp01d(amount), 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_greyscale, &p);
}

// Hue rotation, LUMINANCE-PRESERVING.
//
// RGB is converted to Rec.709 Y'CbCr, the two CHROMA axes are rotated by the
// angle, and it is converted back. The reason to do it this way rather than
// round-tripping through HSV is that HSV's "value" is max(r,g,b), so an HSV hue
// rotation changes how bright the colour looks: rotating a pure red to pure blue
// keeps V=1 while the luminance falls from 0.2126 to 0.0722. Rotating the chroma
// plane leaves Y untouched by construction, which is the property
// tests/test_filter.wyn asserts.
//
// The cost is that the Rec.709 gamut is not a cylinder, so a fully saturated
// colour rotated far enough leaves it and is clamped. That is honest: there is
// no rotation that both preserves luminance and stays in gamut for saturated
// primaries.
static void pf_hue(double* rgb, const FParam* p) {
    double y  = lum709(rgb[0], rgb[1], rgb[2]);
    double cb = (rgb[2] - y) / 1.8556;
    double cr = (rgb[0] - y) / 1.5748;
    double cs = p->a, sn = p->b;               // cos/sin, precomputed
    double cb2 = cb * cs - cr * sn;
    double cr2 = cb * sn + cr * cs;
    rgb[0] = y + 1.5748 * cr2;
    rgb[1] = y - 0.468124 * cr2 - 0.187324 * cb2;
    rgb[2] = y + 1.8556 * cb2;
}

long long wynimg_flt_hue_rotate(void* bufp, double degrees, void* maskp) {
    if (degrees != degrees) return 0;
    double rad = degrees * (3.14159265358979323846 / 180.0);
    FParam p = { cos(rad), sin(rad), 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_hue, &p);
}

// Levels: remap [black, white] to [0, 1], then apply `gamma`.
//
// The gamma is applied AFTER the black/white remap and operates on LINEAR
// values: out = t^(1/gamma), so gamma > 1 brightens midtones, matching the
// familiar midtone slider. Documented rather than pivoted like contrast is,
// because levels is a numeric control -- a user typing 0.5 as a black point
// means the linear value 0.5.
static void pf_levels(double* rgb, const FParam* p) {
    double span = p->b - p->a;
    for (int ch = 0; ch < 3; ch++) {
        double t = clamp01d((rgb[ch] - p->a) / span);
        rgb[ch] = (p->c == 1.0) ? t : pow(t, 1.0 / p->c);
    }
}

long long wynimg_flt_levels(void* bufp, double black, double white,
                            double gamma, void* maskp) {
    if (black != black || white != white || gamma != gamma) return 0;
    // white <= black would divide by zero or invert the ramp; gamma <= 0 has no
    // meaning. Refused, because both are caller errors with no sane reading.
    if (!(white > black)) return 0;
    if (!(gamma > 0.0)) return 0;
    FParam p = { black, white, gamma, 0, NULL };
    return apply_point(bufp, maskp, pf_levels, &p);
}

// Invert, in LINEAR light: out = 1 - in.
//
// Note this is NOT the same picture as inverting 8-bit sRGB bytes, which is what
// most editors do: linear 0.2140 (sRGB 0.5) inverts to linear 0.7860 (sRGB
// 0.906), whereas an sRGB byte invert would give sRGB 0.5 again. Linear is the
// right choice for a linear pipeline -- it is the one that makes invert an
// involution on the values actually stored, and "invert twice is identity" is
// asserted in the tests.
static void pf_invert(double* rgb, const FParam* p) {
    (void)p;
    for (int ch = 0; ch < 3; ch++) rgb[ch] = 1.0 - rgb[ch];
}

long long wynimg_flt_invert(void* bufp, void* maskp) {
    FParam p = { 0.0, 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_invert, &p);
}

// Threshold on Rec.709 luminance: below -> black, at or above -> white.
// Luminance rather than per-channel, because a per-channel threshold turns a
// mid-red into pure red rather than into black or white.
static void pf_threshold(double* rgb, const FParam* p) {
    double v = (lum709(rgb[0], rgb[1], rgb[2]) >= p->a) ? 1.0 : 0.0;
    rgb[0] = v; rgb[1] = v; rgb[2] = v;
}

long long wynimg_flt_threshold(void* bufp, double level, void* maskp) {
    if (level != level) return 0;
    FParam p = { level, 0.0, 0.0, 0, NULL };
    return apply_point(bufp, maskp, pf_threshold, &p);
}

// Posterize to n levels per channel. n=2 gives {0,1}; the endpoints are always
// hit exactly, which is what stops posterize from darkening whites.
static void pf_posterize(double* rgb, const FParam* p) {
    double steps = (double)(p->n - 1);
    for (int ch = 0; ch < 3; ch++)
        rgb[ch] = floor(clamp01d(rgb[ch]) * steps + 0.5) / steps;
}

long long wynimg_flt_posterize(void* bufp, long long levels, void* maskp) {
    if (levels < 2) return 0;
    FParam p = { 0.0, 0.0, 0.0, levels, NULL };
    return apply_point(bufp, maskp, pf_posterize, &p);
}

// ---------------------------------------------------------------------------
// Per-channel curves via a lookup table.
//
// A LUT IS JUST AN N x 1 WYNIMG BUFFER, whose R/G/B channels hold the mapped
// output for each of N equally-spaced inputs in [0,1]. Reusing the buffer type
// means the LUT gets the same generation-tagged-handle safety as everything else
// (a freed curve is a no-op, not a wild read) and needs no second allocator, no
// second free, and no new Wyn type. The A channel is ignored.
//
// Values BETWEEN table entries are linearly interpolated, so a 32-entry table is
// a smooth curve rather than 32 visible bands. A curve is applied to
// UNPREMULTIPLIED colour for the reason in the file header: it is arbitrary and
// non-linear, so it does not commute with the alpha multiply.
// ---------------------------------------------------------------------------

// Fills a LUT with the identity ramp. The starting point for building any curve.
long long wynimg_lut_identity(void* lutp) {
    WynImg* l = wynimg_deref(lutp);
    if (!l || !l->px) return 0;
    long long n = l->w * l->h;
    if (n < 2) return 0;
    for (long long i = 0; i < n; i++) {
        double v = (double)i / (double)(n - 1);
        size_t o = (size_t)i * 4;
        l->px[o + 0] = (float)v;
        l->px[o + 1] = (float)v;
        l->px[o + 2] = (float)v;
        l->px[o + 3] = 1.0f;
    }
    return 1;
}

// Sets one LUT entry. Bounds-checked; out of range is refused, not wrapped.
long long wynimg_lut_set(void* lutp, long long i, double r, double g, double b) {
    WynImg* l = wynimg_deref(lutp);
    if (!l || !l->px) return 0;
    long long n = l->w * l->h;
    if (i < 0 || i >= n) return 0;
    if (r != r || g != g || b != b) return 0;
    size_t o = (size_t)i * 4;
    l->px[o + 0] = (float)clamp01d(r);
    l->px[o + 1] = (float)clamp01d(g);
    l->px[o + 2] = (float)clamp01d(b);
    l->px[o + 3] = 1.0f;
    return 1;
}

// Reads one LUT entry channel. -1.0 when refused (a valid entry is in [0,1], so
// the sentinel cannot be mistaken for data).
double wynimg_lut_get(void* lutp, long long i, long long chan) {
    WynImg* l = wynimg_deref(lutp);
    if (!l || !l->px) return -1.0;
    long long n = l->w * l->h;
    if (i < 0 || i >= n) return -1.0;
    if (chan < 0 || chan > 3) return -1.0;
    return l->px[(size_t)i * 4 + chan];
}

static void pf_curve(double* rgb, const FParam* p) {
    const WynImg* l = p->lut;
    long long n = l->w * l->h;
    double last = (double)(n - 1);
    for (int ch = 0; ch < 3; ch++) {
        double t = clamp01d(rgb[ch]) * last;
        long long i0 = (long long)floor(t);
        if (i0 > n - 2) i0 = n - 2;
        if (i0 < 0) i0 = 0;
        double f = t - (double)i0;
        double a0 = denan(l->px[(size_t)i0 * 4 + ch]);
        double a1 = denan(l->px[(size_t)(i0 + 1) * 4 + ch]);
        rgb[ch] = a0 + (a1 - a0) * f;
    }
}

// Applies a per-channel curve. `lut` is an N x 1 buffer (N >= 2); a stale or
// null LUT handle is REFUSED rather than treated as identity, because "the curve
// you built was freed" must not silently look like a working no-op.
long long wynimg_flt_curve(void* bufp, void* lutp, void* maskp) {
    WynImg* l = wynimg_deref(lutp);
    if (!l || !l->px) return 0;
    if (l->w * l->h < 2) return 0;
    FParam p = { 0.0, 0.0, 0.0, 0, l };
    return apply_point(bufp, maskp, pf_curve, &p);
}

// ---------------------------------------------------------------------------
// Coverage-mask construction and measurement.
// ---------------------------------------------------------------------------

// Writes constant coverage into an axis-aligned rectangle of a mask buffer
// (R channel), clipped to the buffer. This is the minimum a selection needs, and
// it is in C for the same reason every other bulk write is: a 12MP rectangle
// would otherwise be 12M FFI calls.
long long wynimg_flt_cov_rect(void* maskp, long long x0, long long y0,
                              long long w, long long h, double cov) {
    WynImg* m = wynimg_deref(maskp);
    if (!m || !m->px) return 0;
    if (cov != cov) return 0;
    if (w <= 0 || h <= 0) return 0;
    double c = clamp01d(cov);
    for (long long y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= m->h) continue;
        for (long long x = x0; x < x0 + w; x++) {
            if (x < 0 || x >= m->w) continue;
            size_t o = (size_t)(y * m->w + x) * 4;
            m->px[o + 0] = (float)c;
            m->px[o + 3] = 1.0f;     // masks carry coverage in R; alpha is 1
        }
    }
    return 1;
}

// Sum of one channel over the whole buffer, as stored (PREMULTIPLIED).
//
// This exists so a test can assert that a blur CONSERVES ENERGY -- the single
// most useful property of a correctly normalised kernel, and one that no
// per-pixel assertion can express. Summing in C rather than in Wyn is also the
// only option: a 64x64 buffer is 4096 FFI calls, and a real document is
// millions. -1.0 when refused (a real sum of clamped data is never negative).
double wynimg_flt_sum(void* bufp, long long chan) {
    WynImg* im = wynimg_deref(bufp);
    if (!im || !im->px) return -1.0;
    if (chan < 0 || chan > 3) return -1.0;
    long long npx = im->w * im->h;
    double s = 0.0;
    for (long long i = 0; i < npx; i++) s += denan(im->px[(size_t)i * 4 + chan]);
    return s;
}

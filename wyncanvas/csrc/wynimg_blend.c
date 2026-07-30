#include "wynimg.h"
#include <math.h>
#include <stddef.h>   // size_t, used by the compositor's offset arithmetic

// NaN-SAFE, and that is the whole point of the ordering.
//
// The obvious form -- `if (v < 0) return 0; if (v > 1) return 1; return v;` --
// passes NaN through UNTOUCHED, because both comparisons against NaN are false.
// That is not a theoretical input: Wyn computes `0.0/0.0` to NaN in pure Wyn
// (verified) and wynimg_set stores whatever float it is handed, so a NaN
// reaches these kernels from ordinary user code.
//
// The consequence was measured before this guard: 26 of the 27 blend modes
// returned NaN for a NaN operand, wynimg_composite wrote NaN into all four
// channels of a destination pixel, and from there NaN spreads to every later
// composite of that buffer -- a permanent, un-repaintable hole in the document
// that no bounds check can catch, since a NaN pixel is neither <0 nor >1.
//
// Testing `v > 0.0` (rather than `v < 0.0`) makes the false branch the one NaN
// takes, so NaN is mapped to 0 -- fully transparent / black, the same result an
// out-of-range low value gets. Refusing to produce a value is not an option
// here: this returns a double, and there is no error channel.
static double clamp01d(double v) {
    if (!(v > 0.0)) return 0.0;    // NaN lands here
    if (v > 1.0) return 1.0;
    return v;
}

// NaN -> 0, everything else untouched. Distinct from clamp01d because the
// compositor must NOT clamp the values it reads out of a buffer -- an
// un-premultiplied channel can legitimately exceed 1 and clamping it here would
// change the arithmetic for valid data -- while a NaN must still be neutralised
// before it can propagate into the destination and stay there forever.
static double denan(double v) { return (v == v) ? v : 0.0; }

// ColorDodge / ColorBurn as their own functions, because vivid_light is
// *defined* in terms of them and must inherit their guard order exactly.
// The backdrop guard comes FIRST in both: a black backdrop stays black however
// bright the source, and a white backdrop stays white however dark it is.
static double blend_color_dodge(double cb, double cs) {
    if (cb <= 0.0) return 0.0;
    if (cs >= 1.0) return 1.0;
    return cb / (1.0 - cs);
}

static double blend_color_burn(double cb, double cs) {
    if (cb >= 1.0) return 1.0;
    if (cs <= 0.0) return 0.0;
    return 1.0 - (1.0 - cb) / cs;
}

double wynimg_blend_px(long long mode, double cb, double cs) {
    cb = clamp01d(cb);
    cs = clamp01d(cs);
    double r;
    switch (mode) {
        case 0:  r = cs; break;                       // normal
        case 1:  r = cb * cs; break;                  // multiply
        case 2:  r = cb + cs - cb * cs; break;        // screen
        case 3:                                       // overlay
            r = (cb <= 0.5) ? (2.0 * cb * cs)
                            : (1.0 - 2.0 * (1.0 - cb) * (1.0 - cs));
            break;
        case 4:  r = (cb < cs) ? cb : cs; break;      // darken
        case 5:  r = (cb > cs) ? cb : cs; break;      // lighten
        case 6:  r = blend_color_dodge(cb, cs); break; // color_dodge
        case 7:  r = blend_color_burn(cb, cs); break;  // color_burn
        case 8:                                       // hard_light
            r = (cs <= 0.5) ? (2.0 * cb * cs)
                            : (1.0 - 2.0 * (1.0 - cb) * (1.0 - cs));
            break;
        case 9: {                                     // soft_light (W3C)
            if (cs <= 0.5) {
                r = cb - (1.0 - 2.0 * cs) * cb * (1.0 - cb);
            } else {
                double d;
                if (cb <= 0.25) d = ((16.0 * cb - 12.0) * cb + 4.0) * cb;
                else            d = sqrt(cb);
                r = cb + (2.0 * cs - 1.0) * (d - cb);
            }
            break;
        }
        case 10: r = cb + cs; break;                  // linear_dodge
        case 11: r = cb + cs - 1.0; break;            // linear_burn
        case 12: r = cb + 2.0 * cs - 1.0; break;      // linear_light
        case 13:                                      // vivid_light
            // VividLight = ColorBurn(cb, 2cs) below the midpoint,
            // ColorDodge(cb, 2cs-1) above it. Expressed through the shared
            // helpers so the backdrop guard is evaluated first, as those
            // formulas require.
            //
            // The previous form tested `cs` before considering `cb`
            // (`if (cs <= 0.0) r = 0.0; else if (cs >= 1.0) r = 1.0;`), which
            // inverted both corners: a black backdrop went WHITE under a white
            // source and a white backdrop went BLACK under a black source --
            // full-scale error at exactly byte 0 and byte 255, plus a 0->1
            // discontinuity between cs=0.999999 and cs=1.0.
            r = (cs <= 0.5) ? blend_color_burn(cb, 2.0 * cs)
                            : blend_color_dodge(cb, 2.0 * cs - 1.0);
            break;
        case 14: r = fabs(cb - cs); break;            // difference
        case 15: r = cb + cs - 2.0 * cb * cs; break;  // exclusion
        case 16: r = cb - cs; break;                  // subtract
        case 17: r = (cs <= 0.0) ? 1.0 : cb / cs; break; // divide
        case 18:                                      // pin_light
            if (cs <= 0.5) {
                double lo = 2.0 * cs;
                r = (cb < lo) ? cb : lo;
            } else {
                double hi = 2.0 * cs - 1.0;
                r = (cb > hi) ? cb : hi;
            }
            break;
        case 19:                                      // hard_mix
            r = ((cb + cs) >= 1.0) ? 1.0 : 0.0;
            break;
        case 20: r = (cb < cs) ? cb : cs; break;      // darker_color  (per-channel fallback)
        case 21: r = (cb > cs) ? cb : cs; break;      // lighter_color (per-channel fallback)
        // 22-25 non-separable, 26 dissolve: need whole-pixel context.
        case 22: case 23: case 24: case 25: case 26:
        default:
            r = cs; break;
    }
    return clamp01d(r);
}

long long wynimg_composite(void* dstp, void* srcp, void* maskp,
                           long long mode, double opacity) {
    // Handles, not pointers: a stale or freed handle resolves to NULL and takes
    // the null path instead of reading freed memory.
    WynImg* dst = wynimg_deref(dstp);
    WynImg* src = wynimg_deref(srcp);
    WynImg* msk = wynimg_deref(maskp);
    if (!dst || !src || !dst->px || !src->px) return 0;
    if (msk && !msk->px) msk = NULL;
    if (dst->w != src->w || dst->h != src->h) return 0;
    if (msk && (msk->w != dst->w || msk->h != dst->h)) return 0;

    // clamp01d, not a hand-rolled pair of comparisons: a NaN opacity would slip
    // through `if (opacity < 0) ... if (opacity > 1)` untouched and then NaN
    // every alpha in the buffer. Measured before this change: a NaN opacity gave
    // every destination pixel alpha = NaN.
    opacity = clamp01d(opacity);

    long long n = dst->w * dst->h;
    for (long long i = 0; i < n; i++) {
        size_t o = (size_t)i * 4;

        // Source alpha is data, so it can be NaN (wynimg_set stores any float).
        // denan, not clamp01d: an out-of-range-but-finite alpha keeps its
        // existing treatment (the clamp below handles it), while NaN is removed
        // here because it is what breaks the `alpha <= 0.0` guard -- that
        // comparison is FALSE for NaN, so an unclamped NaN alpha sails past the
        // skip and gets written into the destination.
        double sa = denan(src->px[o+3]);
        double alpha = sa * opacity;
        if (msk) {
            // Mask R channel is coverage. It MUST be clamped: a mask whose R
            // channel exceeds 1.0 (trivially producible by wynimg_set, or by a
            // wide-gamut/HDR source) otherwise multiplies alpha above 1.0 and
            // writes an invalid premultiplied pixel. Measured: coverage 4.0 on
            // an unpremultiplied grey 0.25 source stored A=4.0, and the save +
            // reload round trip turned that grey into pure white.
            alpha *= clamp01d(msk->px[o+0]);
        }
        if (alpha <= 0.0) continue;
        alpha = clamp01d(alpha);

        // Destination alpha weights the blend and appears in `1.0 - da`, so a
        // NaN here would NaN the output even for a perfectly valid source.
        double da = denan(dst->px[o+3]);

        // Un-premultiply both sides: blend formulas are defined on
        // unpremultiplied colour. Colour channels are denan'd for the same
        // reason as the alphas -- a single NaN channel in either buffer would
        // otherwise poison all three output channels of this pixel.
        double sr = (sa > 0.0) ? denan(src->px[o+0]) / sa : 0.0;
        double sg = (sa > 0.0) ? denan(src->px[o+1]) / sa : 0.0;
        double sb = (sa > 0.0) ? denan(src->px[o+2]) / sa : 0.0;
        double dr = (da > 0.0) ? denan(dst->px[o+0]) / da : 0.0;
        double dg = (da > 0.0) ? denan(dst->px[o+1]) / da : 0.0;
        double db = (da > 0.0) ? denan(dst->px[o+2]) / da : 0.0;

        // W3C Compositing and Blending Level 1, sec. 9:
        //     Cr = (1 - ab) * Cs + ab * B(Cb, Cs)
        // The blend is weighted BY BACKDROP ALPHA, not switched on it. Where the
        // backdrop is transparent this degenerates to Cs (nothing to blend
        // against); where it is opaque it is the pure blend; in between it
        // fades proportionally.
        //
        // The previous form binary-switched on `da > 0.0`, so any non-zero
        // backdrop alpha -- however slight -- got the full blend. On a feathered
        // 64px edge that was a max error of 0.246 against the reference, 41x the
        // golden test's 0.006 tolerance, with a 0.25 cliff between adjacent
        // pixels. It happened to agree for normal/overlay/hard-light/lighten on
        // the values the unit tests used, which is why all 31 render tests
        // passed over it.
        double br = (1.0 - da) * sr + da * wynimg_blend_px(mode, dr, sr);
        double bg = (1.0 - da) * sg + da * wynimg_blend_px(mode, dg, sg);
        double bb = (1.0 - da) * sb + da * wynimg_blend_px(mode, db, sb);

        // Standard source-over with the blended colour.
        double oa = alpha + da * (1.0 - alpha);
        double orr = br * alpha + dr * da * (1.0 - alpha);
        double og  = bg * alpha + dg * da * (1.0 - alpha);
        double ob  = bb * alpha + db * da * (1.0 - alpha);

        dst->px[o+0] = (float)orr;   // already premultiplied
        dst->px[o+1] = (float)og;
        dst->px[o+2] = (float)ob;
        dst->px[o+3] = (float)oa;
    }
    return 1;
}

long long wynimg_brightness(void* bufp, double amount) {
    WynImg* im = wynimg_deref(bufp);
    if (!im || !im->px) return 0;
    // A NaN amount is REFUSED rather than absorbed. Unlike the compositor, this
    // function has a return channel, and "adjust brightness by NaN" has no
    // sensible interpretation -- silently treating it as 0 would report success
    // for an operation the caller clearly did not mean. Before this guard a NaN
    // amount NaN'd every colour channel in the buffer and still returned 1,
    // because `if (v < 0.0)` and `if (v > 1.0)` are both false for NaN.
    if (amount != amount) return 0;
    long long n = im->w * im->h;
    for (long long i = 0; i < n; i++) {
        size_t o = (size_t)i * 4;
        double a = im->px[o+3];
        if (!(a > 0.0)) continue;     // NaN alpha takes the skip, not the maths
        for (int c = 0; c < 3; c++) {
            // clamp01d, not two bare comparisons: a NaN already stored in the
            // buffer must be normalised, not preserved.
            double v = clamp01d(denan(im->px[o+c]) / a + amount);
            im->px[o+c] = (float)(v * a);          // re-premultiply
        }
    }
    return 1;
}

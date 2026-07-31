// wynimg_select - the selection engine.
//
// A SELECTION IS A FLOAT32 COVERAGE MASK THE SIZE OF THE DOCUMENT, 0..1 per
// pixel. It is NOT a rectangle, and that is the entire design.
//
// A rectangle-shaped selection cannot represent a feathered edge, a lasso, or
// an antialiased circle, so an editor built on rectangles grows a second
// representation the moment any of those is added - and then a second clipping
// path for each tool. Coverage collapses all of it into one code path: a
// fractional rect edge, a feathered border, an antialiased ellipse rim and a
// lasso are the same numbers in the same buffer, and "how much of this pixel is
// selected" has one answer that every consumer already knows how to use.
//
// It is also ALREADY the representation this project uses twice over: the brush
// accumulates a coverage buffer (csrc/wynimg_paint.c), text rasterises into one
// (csrc/wynimg_text.c), and a layer mask's R channel is read as linear coverage
// by wynimg_composite. So a selection composes with all three by multiplication
// and needs no new compositing rule - `wynimg_sel_clip` is the whole
// integration, and it is four lines.
//
// FILL RULE: NONZERO WINDING, not even-odd. Documented and tested: for a
// self-intersecting pentagram the centre is INSIDE under nonzero (winding 2)
// and a HOLE under even-odd. Nonzero is chosen because a lasso is a gesture, not
// a glyph: a user who crosses their own path expects the enclosed area to stay
// selected rather than punching a hole they did not ask for. (Even-odd is the
// right default for fonts, where a counter IS a hole and the outer/inner
// contours are drawn in the same direction.) tests/test_select.wyn asserts the
// pentagram centre is 1.0, so switching the rule turns that test red.
//
// EVERY SHAPE GOES THROUGH ONE COMBINE KERNEL. Each op rasterises the shape into
// a scratch coverage buffer (allocated with wynimg_new, so it is a handle like
// everything else) and then combines the whole buffer with the current
// selection. That costs one document-sized allocation per op, and it is worth
// it: `intersect` and `replace` must write pixels the shape never touches (a
// pixel outside the new shape becomes UNSELECTED), so a "rasterise directly with
// the op" fast path needs two of the four ops to clear the rest of the buffer
// anyway - and would be four places to get max/multiply wrong instead of one.
//
// FFI CONTRACT, as everywhere in this shim: Wyn `int` <-> C `long long`,
// Wyn `float` <-> C `double`. Handles arrive as `void*` and are resolved through
// wynimg_deref(), so a stale handle is a safe no-op and never a use-after-free.
#include "wynimg.h"

#include <math.h>
#include <stdlib.h>

// Prototypes live here rather than in csrc/wynimg.h: this translation unit is
// self-contained (nothing else in the shim calls into it), and the declarations
// that matter across the boundary are the ones in src/select.wyn.

// Boolean ops. The numbering is part of the FFI contract with src/select.wyn.
#define WYNIMG_SEL_REPLACE   0
#define WYNIMG_SEL_ADD       1
#define WYNIMG_SEL_SUBTRACT  2
#define WYNIMG_SEL_INTERSECT 3

long long wynimg_sel_combine(void* dstp, void* srcp, long long op);
long long wynimg_sel_all(void* selp);
long long wynimg_sel_none(void* selp);
long long wynimg_sel_invert(void* selp);

long long wynimg_sel_rect(void* selp, double x, double y, double w, double h,
                          long long op);
long long wynimg_sel_ellipse(void* selp, double cx, double cy,
                             double rx, double ry, long long op);

long long wynimg_poly_new(long long npts);
long long wynimg_poly_set(void* ptsp, long long i, double x, double y);
long long wynimg_poly_count(void* ptsp);
long long wynimg_sel_polygon(void* selp, void* ptsp, long long op);

long long wynimg_sel_feather(void* selp, double radius, long long passes);
long long wynimg_sel_grow(void* selp, long long amount);

double    wynimg_sel_at(void* selp, long long x, long long y);
double    wynimg_sel_sum(void* selp);
double    wynimg_sel_max(void* selp);
long long wynimg_sel_is_empty(void* selp);
long long wynimg_sel_bbox(void* selp, long long which);
long long wynimg_sel_clip(void* covp, void* selp);
long long wynimg_sel_outline(void* selp, void* outp, double thresh);

// Supersampling grid for the shapes whose coverage has no closed form here
// (ellipse, polygon): 4x4 = 16 samples at pixel-cell centres, so a boundary
// pixel's coverage is a multiple of 1/16 and every test value is derivable by
// hand rather than by running the code. A rect does NOT use this - its overlap
// area is exact, see wynimg_sel_rect.
#define SEL_SS 4

static double clamp01d(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

// ---------------------------------------------------------------------------
// The combine kernel: the ONE place a boolean op is defined.
// ---------------------------------------------------------------------------

// UNION IS max(), NOT a+b-ab.
//
// Both agree exactly on hard masks. They differ on soft edges, and max is the
// right one: adding a 0.5 feathered edge to an already-0.5 selection under the
// probabilistic union gives 0.75, i.e. "add to selection" would darken every
// edge it touched a second time - the same accumulation bug csrc/wynimg_paint.c
// avoids by combining stroke segments with max. Selectedness is not a
// probability; it is a fraction of a pixel, and the union of two overlapping
// fractions is at most the larger one's worth of certainty.
//
// SUBTRACT is d*(1-s) and INTERSECT is d*s, which are the standard soft-mask
// forms and are exact complements: subtract(s) == intersect(invert(s)).
static float combine1(float d, float s, long long op) {
    switch (op) {
        case WYNIMG_SEL_ADD:       return d > s ? d : s;
        case WYNIMG_SEL_SUBTRACT:  return (float)((double)d * (1.0 - (double)s));
        case WYNIMG_SEL_INTERSECT: return (float)((double)d * (double)s);
        default:                   return s;              // REPLACE
    }
}

// Combines `src` coverage into `dst` for the whole buffer. Buffers must share
// dimensions. 1 on success, 0 on a null/stale handle or a mismatch.
long long wynimg_sel_combine(void* dstp, void* srcp, long long op) {
    WynImg* d = wynimg_deref(dstp);
    WynImg* s = wynimg_deref(srcp);
    if (!d || !s || !d->px || !s->px) return 0;
    if (d->w != s->w || d->h != s->h) return 0;
    if (op < 0 || op > WYNIMG_SEL_INTERSECT) return 0;
    long long n = d->w * d->h;
    for (long long i = 0; i < n; i++) {
        size_t o = (size_t)i * 4;
        d->px[o] = combine1(d->px[o], (float)clamp01d((double)s->px[o]), op);
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Whole-buffer coverage.
// ---------------------------------------------------------------------------

// Only the R channel carries coverage, matching the brush and the layer mask.
// The other three are left alone deliberately: a selection buffer is never
// composited as colour, and writing 1.0 into alpha would make a stray
// wynimg_composite of a selection produce white instead of nothing.
static void fill_cov(WynImg* im, double v) {
    long long n = im->w * im->h;
    for (long long i = 0; i < n; i++) im->px[(size_t)i * 4] = (float)v;
}

long long wynimg_sel_all(void* selp) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    fill_cov(s, 1.0);
    return 1;
}

long long wynimg_sel_none(void* selp) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    fill_cov(s, 0.0);
    return 1;
}

// Exactly 1-c, so invert is an involution on every value including partial
// coverage: invert twice is bit-identical, which tests/test_select.wyn asserts
// with wynimg_max_diff == 0. A threshold-based "invert" would not be.
long long wynimg_sel_invert(void* selp) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    long long n = s->w * s->h;
    for (long long i = 0; i < n; i++) {
        size_t o = (size_t)i * 4;
        s->px[o] = (float)(1.0 - clamp01d((double)s->px[o]));
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Shapes.
//
// Each rasterises into a scratch buffer and then combines. The scratch is a
// wynimg handle so it obeys the same allocation cap and the same failure mode as
// every other buffer; it is freed before returning, always.
// ---------------------------------------------------------------------------

static void* scratch_for(const WynImg* s) { return wynimg_new(s->w, s->h); }

// Coverage of pixel column [px, px+1) by the interval [a, b). Exact, not
// sampled - a box filter over an axis-aligned edge has a closed form, and using
// it means a rect at x=16.5 puts exactly 0.5 in pixel 16 rather than 8/16.
static double span_overlap(double a, double b, long long p) {
    double lo = (double)p, hi = lo + 1.0;
    double s = a > lo ? a : lo;
    double e = b < hi ? b : hi;
    return e > s ? e - s : 0.0;
}

// Antialiased rectangle. (x,y) is the top-left corner and (w,h) the size, both
// in continuous document coordinates, so fractional edges are meaningful.
// Coverage is the EXACT area of the pixel square inside the rect, which for an
// axis-aligned rect is separable: overlap_x * overlap_y.
long long wynimg_sel_rect(void* selp, double x, double y, double w, double h,
                          long long op) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    if (op < 0 || op > WYNIMG_SEL_INTERSECT) return 0;
    // A zero or negative size is an empty shape, not an error: `replace` with it
    // is a legitimate "select nothing", and `intersect` with it legitimately
    // empties the selection. Both fall out of combining an all-zero scratch.
    void* h2 = scratch_for(s);
    WynImg* t = wynimg_deref(h2);
    if (!t) return 0;

    if (w > 0.0 && h > 0.0) {
        double x1 = x + w, y1 = y + h;
        long long xa = (long long)floor(x), xb = (long long)ceil(x1);
        long long ya = (long long)floor(y), yb = (long long)ceil(y1);
        if (xa < 0) xa = 0;
        if (ya < 0) ya = 0;
        if (xb > t->w) xb = t->w;
        if (yb > t->h) yb = t->h;
        for (long long py = ya; py < yb; py++) {
            double cy = span_overlap(y, y1, py);
            if (cy <= 0.0) continue;
            for (long long px = xa; px < xb; px++) {
                double cx = span_overlap(x, x1, px);
                if (cx <= 0.0) continue;
                t->px[(size_t)(py * t->w + px) * 4] = (float)(cx * cy);
            }
        }
    }

    long long ok = wynimg_sel_combine(selp, h2, op);
    wynimg_free(h2);
    return ok;
}

// Antialiased ellipse, axis-aligned, centred on (cx,cy) with radii (rx,ry).
// Coverage by 4x4 supersampling: an ellipse's exact pixel overlap area has no
// cheap closed form, and 16 samples put the rim within 1/16 of the truth while
// keeping every test value hand-derivable.
long long wynimg_sel_ellipse(void* selp, double cx, double cy,
                             double rx, double ry, long long op) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    if (op < 0 || op > WYNIMG_SEL_INTERSECT) return 0;
    void* h2 = scratch_for(s);
    WynImg* t = wynimg_deref(h2);
    if (!t) return 0;

    if (rx > 0.0 && ry > 0.0) {
        long long xa = (long long)floor(cx - rx), xb = (long long)ceil(cx + rx);
        long long ya = (long long)floor(cy - ry), yb = (long long)ceil(cy + ry);
        if (xa < 0) xa = 0;
        if (ya < 0) ya = 0;
        if (xb > t->w - 1) xb = t->w - 1;
        if (yb > t->h - 1) yb = t->h - 1;
        double step = 1.0 / (double)SEL_SS;
        double half = step * 0.5;
        for (long long py = ya; py <= yb; py++) {
            for (long long px = xa; px <= xb; px++) {
                int hits = 0;
                for (int sy = 0; sy < SEL_SS; sy++) {
                    double fy = ((double)py + half + step * (double)sy - cy) / ry;
                    for (int sx = 0; sx < SEL_SS; sx++) {
                        double fx = ((double)px + half + step * (double)sx - cx) / rx;
                        if (fx * fx + fy * fy <= 1.0) hits++;
                    }
                }
                if (hits)
                    t->px[(size_t)(py * t->w + px) * 4] =
                        (float)((double)hits / (double)(SEL_SS * SEL_SS));
            }
        }
    }

    long long ok = wynimg_sel_combine(selp, h2, op);
    wynimg_free(h2);
    return ok;
}

// ---------------------------------------------------------------------------
// Lasso / polygon.
//
// THE POINT LIST IS A wynimg BUFFER, not a Wyn array. A Wyn array cell is a
// 16-byte tagged union and bulk data never crosses the FFI boundary in this
// project, so a 400-point lasso would be 400 boxed values and 800 FFI calls to
// unpack. An n-by-1 buffer holding x in R and y in G reuses the handle table,
// the allocation cap and the generation tagging that already exist - a stale
// point list is a safe no-op like every other stale handle.
// ---------------------------------------------------------------------------

long long wynimg_poly_new(long long npts) {
    if (npts < 1) return 0;
    return (long long)wynimg_new(npts, 1);
}

long long wynimg_poly_set(void* ptsp, long long i, double x, double y) {
    WynImg* p = wynimg_deref(ptsp);
    if (!p || !p->px) return 0;
    if (i < 0 || i >= p->w) return 0;
    p->px[(size_t)i * 4 + 0] = (float)x;
    p->px[(size_t)i * 4 + 1] = (float)y;
    return 1;
}

long long wynimg_poly_count(void* ptsp) {
    WynImg* p = wynimg_deref(ptsp);
    return p ? p->w : 0;
}

// Winding number of (x,y) with respect to the closed polygon. The classic
// crossing test with DIRECTION: an upward edge crossing the ray counts +1, a
// downward one -1. Nonzero winding => inside.
//
// The half-open comparison (y0 <= y < y1 on the way up, y1 <= y < y0 on the way
// down) is what stops a vertex exactly on the sample row from being counted
// twice, which would flip the parity of a whole scanline.
static int winding(const float* pt, long long n, double x, double y) {
    int wn = 0;
    for (long long i = 0; i < n; i++) {
        long long j = (i + 1 == n) ? 0 : i + 1;
        double x0 = pt[(size_t)i * 4 + 0], y0 = pt[(size_t)i * 4 + 1];
        double x1 = pt[(size_t)j * 4 + 0], y1 = pt[(size_t)j * 4 + 1];
        // Cross product of the edge with the vector to the sample: > 0 means the
        // sample is left of the edge.
        double side = (x1 - x0) * (y - y0) - (x - x0) * (y1 - y0);
        if (y0 <= y) {
            if (y1 > y && side > 0.0) wn++;
        } else {
            if (y1 <= y && side < 0.0) wn--;
        }
    }
    return wn;
}

// Fills the polygon in `ptsp` (>= 3 points, implicitly closed) with 4x4
// supersampled coverage under the NONZERO rule - see the file header for why
// nonzero and not even-odd.
//
// Cost is 16 samples x bbox area x edge count. That is honest brute force: a
// scanline/active-edge rasteriser would be O(edges log edges + area) and is the
// right thing for a 400-point lasso on a 12MP document, but it is also where a
// fill-rule bug hides, and this engine's first job is to be provably correct
// about the rule. Documented as a known cost rather than silently slow.
long long wynimg_sel_polygon(void* selp, void* ptsp, long long op) {
    WynImg* s = wynimg_deref(selp);
    WynImg* p = wynimg_deref(ptsp);
    if (!s || !s->px) return 0;
    if (!p || !p->px) return 0;
    if (op < 0 || op > WYNIMG_SEL_INTERSECT) return 0;
    long long n = p->w;
    if (n < 3) return 0;                  // not an area

    void* h2 = scratch_for(s);
    WynImg* t = wynimg_deref(h2);
    if (!t) return 0;

    double lo_x = p->px[0], hi_x = p->px[0];
    double lo_y = p->px[1], hi_y = p->px[1];
    for (long long i = 1; i < n; i++) {
        double px = p->px[(size_t)i * 4 + 0], py = p->px[(size_t)i * 4 + 1];
        if (px < lo_x) lo_x = px;
        if (px > hi_x) hi_x = px;
        if (py < lo_y) lo_y = py;
        if (py > hi_y) hi_y = py;
    }
    long long xa = (long long)floor(lo_x), xb = (long long)ceil(hi_x);
    long long ya = (long long)floor(lo_y), yb = (long long)ceil(hi_y);
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > t->w - 1) xb = t->w - 1;
    if (yb > t->h - 1) yb = t->h - 1;

    double step = 1.0 / (double)SEL_SS;
    double half = step * 0.5;
    for (long long py = ya; py <= yb; py++) {
        for (long long px = xa; px <= xb; px++) {
            int hits = 0;
            for (int sy = 0; sy < SEL_SS; sy++) {
                double y = (double)py + half + step * (double)sy;
                for (int sx = 0; sx < SEL_SS; sx++) {
                    double x = (double)px + half + step * (double)sx;
                    if (winding(p->px, n, x, y) != 0) hits++;
                }
            }
            if (hits)
                t->px[(size_t)(py * t->w + px) * 4] =
                    (float)((double)hits / (double)(SEL_SS * SEL_SS));
        }
    }

    long long ok = wynimg_sel_combine(selp, h2, op);
    wynimg_free(h2);
    return ok;
}

// ---------------------------------------------------------------------------
// Feather - a SEPARABLE box blur.
//
// Separable because a 2D kernel at radius 20 is (41*41)=1681 taps per pixel
// against 2*41=82 for two 1D passes: 20x the work for the identical result,
// which on a 12MP document is the difference between interactive and not.
//
// Each 1D pass is O(1) per pixel via a prefix sum, so the cost does not even
// grow with the radius. Three passes of a box blur approximate a gaussian to
// within about 3% (the central limit theorem doing the work); one pass is
// exposed too, because a single box has an exactly linear ramp whose values a
// test can derive by counting pixels - and a feather whose numbers cannot be
// predicted is a feather nobody can review.
//
// Edges CLAMP (the border pixel is replicated). The alternative, treating
// outside as zero, would make a selection that touches the canvas edge fade out
// along that edge, which is never what a user means.
// ---------------------------------------------------------------------------

// Sum of v[clamp(j,0,n-1)] for j in [a,b], from a prefix table of length n+1.
static double win_sum(const double* pre, long long n, long long a, long long b,
                      double v0, double vn) {
    double s = 0.0;
    if (a < 0) { s += v0 * (double)(-a); a = 0; }
    if (b > n - 1) { s += vn * (double)(b - (n - 1)); b = n - 1; }
    if (a <= b) s += pre[b + 1] - pre[a];
    return s;
}

// One separable box pass: rows of `src` into `dst`, then columns of `dst` back
// into `src`. `pre` is scratch of max(w,h)+1 doubles.
static void box_pass(float* a, float* b, long long w, long long h, long long r,
                     double* pre) {
    double norm = 1.0 / (double)(2 * r + 1);
    for (long long y = 0; y < h; y++) {
        const float* row = a + y * w;
        pre[0] = 0.0;
        for (long long x = 0; x < w; x++) pre[x + 1] = pre[x] + (double)row[x];
        for (long long x = 0; x < w; x++)
            b[y * w + x] = (float)(win_sum(pre, w, x - r, x + r,
                                           (double)row[0], (double)row[w - 1]) * norm);
    }
    for (long long x = 0; x < w; x++) {
        pre[0] = 0.0;
        for (long long y = 0; y < h; y++) pre[y + 1] = pre[y] + (double)b[y * w + x];
        double v0 = (double)b[x], vn = (double)b[(h - 1) * w + x];
        for (long long y = 0; y < h; y++)
            a[y * w + x] = (float)(win_sum(pre, h, y - r, y + r, v0, vn) * norm);
    }
}

// Blurs the R channel in place. `passes` is clamped to 1..8. A radius that
// rounds to 0 is a no-op and reports success - "feather by 0.4px" is a request
// with a correct empty answer, not an error.
long long wynimg_sel_feather(void* selp, double radius, long long passes) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    if (!(radius == radius)) return 0;                    // NaN
    long long r = (long long)floor(radius + 0.5);
    if (r < 1) return 1;
    if (passes < 1) passes = 1;
    if (passes > 8) passes = 8;

    long long w = s->w, h = s->h, n = w * h;
    long long m = (w > h ? w : h) + 1;
    float*  a   = (float*)malloc((size_t)n * sizeof(float));
    float*  b   = (float*)malloc((size_t)n * sizeof(float));
    double* pre = (double*)malloc((size_t)m * sizeof(double));
    if (!a || !b || !pre) { free(a); free(b); free(pre); return 0; }

    for (long long i = 0; i < n; i++) a[i] = (float)clamp01d((double)s->px[(size_t)i * 4]);
    for (long long p = 0; p < passes; p++) box_pass(a, b, w, h, r, pre);
    for (long long i = 0; i < n; i++) s->px[(size_t)i * 4] = a[i];

    free(a); free(b); free(pre);
    return 1;
}

// Grow (amount > 0) or contract (amount < 0) by |amount| pixels.
//
// THE APPROXIMATION, STATED PLAINLY. This is a THRESHOLD SHIFT on a box-blurred
// copy, not a Euclidean distance transform:
//
//   grow n     = clamp01(sum of coverage in the (2n+1)^2 neighbourhood)
//   contract n = the same applied to the complement, i.e. 1 - grow(1 - cov)
//
// Two consequences, both deliberate and both cheap to reason about:
//
//   * The neighbourhood is a SQUARE (Chebyshev metric), because that is what a
//     separable kernel gives. A circle grown by 10 comes back with slightly
//     flattened diagonals; a true disc needs a real distance transform, which is
//     a bigger piece of machinery than this engine needs today.
//   * For a HARD mask this is EXACT morphological dilation/erosion: a pixel with
//     any fully-selected pixel within n becomes selected, and a pixel with any
//     fully-unselected pixel within n becomes unselected. tests/test_select.wyn
//     asserts the boundary lands exactly n pixels out and in.
//   * For a SOFT (feathered) mask it moves the edge AND hardens it, because
//     clamping the neighbourhood sum saturates. Feather AFTER growing, not
//     before, if the softness matters.
long long wynimg_sel_grow(void* selp, long long amount) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0;
    if (amount == 0) return 1;
    long long n2 = amount < 0 ? -amount : amount;
    long long np = s->w * s->h;

    // Erosion is dilation of the complement, so one kernel does both. Inverting
    // in place around the call is cheaper than a second blur variant and cannot
    // drift from it.
    if (amount < 0) wynimg_sel_invert(selp);
    if (!wynimg_sel_feather(selp, (double)n2, 1)) {
        if (amount < 0) wynimg_sel_invert(selp);
        return 0;
    }
    double area = (double)((2 * n2 + 1) * (2 * n2 + 1));
    for (long long i = 0; i < np; i++) {
        size_t o = (size_t)i * 4;
        s->px[o] = (float)clamp01d((double)s->px[o] * area);
    }
    if (amount < 0) wynimg_sel_invert(selp);
    return 1;
}

// ---------------------------------------------------------------------------
// Queries. In C because a Wyn-side scan of a 12MP mask would be 12M FFI calls.
// ---------------------------------------------------------------------------

double wynimg_sel_at(void* selp, long long x, long long y) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0.0;
    if (x < 0 || y < 0 || x >= s->w || y >= s->h) return 0.0;
    return (double)s->px[(size_t)(y * s->w + x) * 4];
}

// Total selected area in pixels. This is what makes "feather approximately
// conserves coverage" checkable: a blur that is not normalised, or that is
// applied twice, changes this number.
double wynimg_sel_sum(void* selp) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0.0;
    long long n = s->w * s->h;
    double acc = 0.0;
    for (long long i = 0; i < n; i++) acc += (double)s->px[(size_t)i * 4];
    return acc;
}

double wynimg_sel_max(void* selp) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 0.0;
    long long n = s->w * s->h;
    double worst = 0.0;
    for (long long i = 0; i < n; i++) {
        double v = (double)s->px[(size_t)i * 4];
        if (v > worst) worst = v;
    }
    return worst;
}

// Empty means NO pixel has any coverage at all. Not "sum is small": a one-pixel
// selection is not empty, and a caller that skips the whole document because the
// area rounded to nothing would silently drop the user's work.
long long wynimg_sel_is_empty(void* selp) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return 1;                 // no selection == nothing selected
    long long n = s->w * s->h;
    for (long long i = 0; i < n; i++)
        if (s->px[(size_t)i * 4] > 0.0f) return 0;
    return 1;
}

// Inclusive bounding box of every pixel with coverage > 0: which = 0,1,2,3 ->
// x0, y0, x1, y1. Returns -1 for all four when the selection is empty.
//
// The threshold is "> 0", not "> 0.5", precisely because callers use this to
// SKIP rows: a row that is only 3% selected still has to be processed, and a
// bbox that excluded it would clip the feathered border off every operation.
//
// One function with a selector rather than four near-identical ones, so the
// scan and the emptiness rule cannot drift between them.
long long wynimg_sel_bbox(void* selp, long long which) {
    WynImg* s = wynimg_deref(selp);
    if (!s || !s->px) return -1;
    long long x0 = s->w, y0 = s->h, x1 = -1, y1 = -1;
    for (long long y = 0; y < s->h; y++) {
        for (long long x = 0; x < s->w; x++) {
            if (s->px[(size_t)(y * s->w + x) * 4] <= 0.0f) continue;
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
            if (y < y0) y0 = y;
            if (y > y1) y1 = y;
        }
    }
    if (x1 < 0) return -1;
    switch (which) {
        case 0: return x0;
        case 1: return y0;
        case 2: return x1;
        default: return y1;
    }
}

// Multiplies a coverage buffer (a brush stroke, a text layout, a fill region) by
// the selection. THIS IS THE WHOLE INTEGRATION with the rest of the editor: any
// tool that already produces coverage becomes selection-aware by one call before
// wynimg_stroke_apply, with no change to the compositing kernel and no second
// clipping path. Buffers must share dimensions. 1 / 0.
long long wynimg_sel_clip(void* covp, void* selp) {
    return wynimg_sel_combine(covp, selp, WYNIMG_SEL_INTERSECT);
}

// ---------------------------------------------------------------------------
// Marching ants: the selection's BOUNDARY, as a coverage buffer.
// ---------------------------------------------------------------------------

// Write the outline of `sel` into `out`, as coverage.
//
// WHY THIS IS IN C AT ALL. The editor drew a bounding BOX for every selection,
// so an ellipse showed the rectangle it was inscribed in and a feathered edge
// showed nothing of its shape. The honest reason recorded in src/ui.wyn was that
// finding edge pixels from Wyn means one wynimg_sel_at call PER PIXEL, per frame
// - 65,536 FFI calls for a 256x256 document, which is the "one Win_rect per
// pixel" mistake that file's header rejects. That reason is sound, and it is an
// argument against doing it in WYN, not against doing it. One pass in C is a
// single call.
//
// WHAT AN OUTLINE PIXEL IS. A pixel is on the boundary when it differs from at
// least one 4-neighbour by more than `thresh`. The written value is the LARGEST
// such difference, not 1.0, which is what makes a soft edge legible: a feathered
// selection's boundary fades exactly as the coverage gradient does, and a hard
// edge comes out at full strength. Taking max (rather than summing the four
// differences) keeps the result in [0,1] without a clamp that would flatten
// every corner - a corner differs from two neighbours at once and would
// otherwise saturate while a straight edge did not.
//
// 4-neighbour, not 8: a diagonal-inclusive test widens every boundary to two
// pixels on a 45-degree edge, which reads as a blurry selection rather than a
// crisp one.
//
// THE DOCUMENT EDGE COUNTS AS OUTSIDE. A selection that runs to the border is
// outlined along that border. Treating the edge as "same as me" instead would
// silently drop the outline exactly where select-all puts it, which is the most
// common selection there is.
//
// `out` is REPLACED, not combined: it is a scratch overlay owned by the caller
// and rebuilt whenever the selection changes, so accumulating into it would keep
// stale ants from a previous shape. Buffers must share dimensions. 1 / 0.
long long wynimg_sel_outline(void* selp, void* outp, double thresh) {
    WynImg* s = wynimg_deref(selp);
    WynImg* o = wynimg_deref(outp);
    if (!s || !s->px || !o || !o->px) return 0;
    if (s->w != o->w || s->h != o->h) return 0;
    if (thresh < 0.0) thresh = 0.0;

    const long long w = s->w;
    const long long h = s->h;

    for (long long y = 0; y < h; y++) {
        for (long long x = 0; x < w; x++) {
            const size_t here = (size_t)(y * w + x) * 4;
            const double c = (double)s->px[here];

            // Off-document neighbours read as 0 - see the edge note above.
            const double left  = (x > 0)     ? (double)s->px[(size_t)(y * w + x - 1) * 4] : 0.0;
            const double right = (x < w - 1) ? (double)s->px[(size_t)(y * w + x + 1) * 4] : 0.0;
            const double up    = (y > 0)     ? (double)s->px[(size_t)((y - 1) * w + x) * 4] : 0.0;
            const double down  = (y < h - 1) ? (double)s->px[(size_t)((y + 1) * w + x) * 4] : 0.0;

            double d = c - left;  if (d < 0.0) d = -d;
            double t = c - right; if (t < 0.0) t = -t;  if (t > d) d = t;
            t = c - up;           if (t < 0.0) t = -t;  if (t > d) d = t;
            t = c - down;         if (t < 0.0) t = -t;  if (t > d) d = t;

            const float v = (d > thresh) ? (float)clamp01d(d) : 0.0f;
            // Written to all four channels so the same buffer can be uploaded as
            // a texture or read back as coverage without a second convention.
            o->px[here]     = v;
            o->px[here + 1] = v;
            o->px[here + 2] = v;
            o->px[here + 3] = v;
        }
    }
    return 1;
}

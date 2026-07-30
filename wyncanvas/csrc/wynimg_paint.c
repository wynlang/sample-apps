// wynimg_paint - painting kernels: brush strokes, flood fill, buffer copies.
//
// WHY A STROKE IS NOT A SEQUENCE OF DABS.
//
// The obvious brush is "composite a soft disc at every mouse position". It is
// wrong, and visibly so: consecutive positions overlap, so each overlapping
// pixel is composited twice or ten times and a 30%-alpha stroke turns opaque
// wherever the pointer moved slowly. Photoshop calls the correct behaviour
// "build-up off", and it is the default everywhere for a reason.
//
// So a stroke here is TWO buffers and one composite:
//
//   1. a COVERAGE buffer (`stroke`), in which each segment writes
//      max(existing, coverage) - so overlap saturates instead of accumulating;
//   2. the layer's pixels as they were BEFORE the stroke began (`base`).
//
// Every frame the layer is recomputed as base + colour*coverage. That makes the
// live preview identical to the final result, makes the whole stroke exactly one
// undoable operation, and makes stroke alpha mean what it says.
//
// COVERAGE IS A DISTANCE FIELD, NOT A STAMP.
//
// wynimg_stroke_seg computes, for each pixel in the segment's bounding box, the
// distance to the SEGMENT (not to its endpoints) and turns that into coverage.
// A straight drag therefore produces one uniform capsule, not a bumpy chain,
// regardless of how fast the pointer moved - a dab-based brush leaves gaps at
// speed and lumps at rest.
//
// PREMULTIPLIED LINEAR THROUGHOUT.
//
// Colours arrive as UNPREMULTIPLIED LINEAR-light (the caller runs
// pixel.srgb_to_linear on whatever the colour picker produced) and are stored
// premultiplied, which is what every other buffer in this project holds. The
// eraser is destination-out - it scales all four channels, including alpha,
// which is the only way to erase in a premultiplied buffer without leaving a
// coloured halo where alpha went to zero but RGB did not.
#include "wynimg.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static double clamp01d(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

// ---------------------------------------------------------------------------
// Buffer copies. Needed by the stroke model (the "before" snapshot) and by
// undo/redo, which restores a snapshot into the live layer buffer rather than
// swapping handles - layers hold handles, and swapping them would strand every
// other holder of the old one.
// ---------------------------------------------------------------------------

// A new buffer with the same dimensions and contents. 0 on failure.
void* wynimg_clone(void* p) {
    WynImg* s = wynimg_deref(p);
    if (!s || !s->px) return NULL;
    void* h = wynimg_new(s->w, s->h);
    WynImg* d = wynimg_deref(h);
    if (!d || !d->px) return NULL;
    memcpy(d->px, s->px, (size_t)(s->w * s->h * 4) * sizeof(float));
    return h;
}

// Overwrites dst with src. Dimensions must match. 1 ok, 0 refused.
long long wynimg_copy_into(void* dstp, void* srcp) {
    WynImg* d = wynimg_deref(dstp);
    WynImg* s = wynimg_deref(srcp);
    if (!d || !s || !d->px || !s->px) return 0;
    if (d->w != s->w || d->h != s->h) return 0;
    memcpy(d->px, s->px, (size_t)(s->w * s->h * 4) * sizeof(float));
    return 1;
}

// ---------------------------------------------------------------------------
// Coverage.
// ---------------------------------------------------------------------------

// Distance from (px,py) to the segment (ax,ay)-(bx,by). Degenerates to the
// point distance when the segment has zero length, which is what a single click
// is.
static double seg_dist(double px, double py,
                       double ax, double ay, double bx, double by) {
    double vx = bx - ax, vy = by - ay;
    double wx = px - ax, wy = py - ay;
    double vv = vx * vx + vy * vy;
    double t = 0.0;
    if (vv > 0.0) {
        t = (wx * vx + wy * vy) / vv;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
    }
    double dx = wx - t * vx, dy = wy - t * vy;
    return sqrt(dx * dx + dy * dy);
}

// Coverage at distance `d` for a brush of `radius` whose inner `hardness`
// fraction is solid. Smoothstepped across the falloff so the edge has no
// visible banding; hardness 1 still gets a one-pixel antialiased rim, because a
// hard-edged disc in a float buffer is a staircase.
static double falloff(double d, double radius, double hardness) {
    if (radius <= 0.0) return 0.0;
    double inner = radius * clamp01d(hardness);
    if (inner > radius - 0.5) inner = radius - 0.5;   // always antialias the rim
    if (inner < 0.0) inner = 0.0;
    if (d <= inner) return 1.0;
    if (d >= radius) return 0.0;
    double t = (d - inner) / (radius - inner);        // 0 at inner, 1 at rim
    double c = 1.0 - t;
    return c * c * (3.0 - 2.0 * c);                   // smoothstep
}

// Accumulates one segment of a stroke into the coverage buffer `stroke`
// (R channel = coverage), combining with max so overlap saturates.
//
// Only the segment's bounding box, grown by the radius, is touched - a stroke
// on a 12MP document must cost the brush's area, not the document's.
long long wynimg_stroke_seg(void* strokep,
                            double x0, double y0, double x1, double y1,
                            double radius, double hardness) {
    WynImg* s = wynimg_deref(strokep);
    if (!s || !s->px) return 0;
    if (radius <= 0.0) return 0;

    double lo_x = (x0 < x1 ? x0 : x1) - radius - 1.0;
    double hi_x = (x0 > x1 ? x0 : x1) + radius + 1.0;
    double lo_y = (y0 < y1 ? y0 : y1) - radius - 1.0;
    double hi_y = (y0 > y1 ? y0 : y1) + radius + 1.0;

    long long xa = (long long)floor(lo_x), xb = (long long)ceil(hi_x);
    long long ya = (long long)floor(lo_y), yb = (long long)ceil(hi_y);
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > s->w - 1) xb = s->w - 1;
    if (yb > s->h - 1) yb = s->h - 1;

    for (long long y = ya; y <= yb; y++) {
        for (long long x = xa; x <= xb; x++) {
            // Pixel CENTRES, so a click at an integer coordinate produces a
            // symmetric dab rather than one biased half a pixel up and left.
            double d = seg_dist((double)x + 0.5, (double)y + 0.5,
                                x0, y0, x1, y1);
            double c = falloff(d, radius, hardness);
            if (c <= 0.0) continue;
            size_t o = (size_t)(y * s->w + x) * 4;
            if (c > (double)s->px[o]) s->px[o] = (float)c;
        }
    }
    return 1;
}

// dst = base composited with (colour, alpha) masked by the stroke coverage.
//
// `erase` selects destination-out instead of source-over. All three buffers must
// share dimensions; `base` may be the same handle as `dst` for a destructive
// apply, though the stroke model always passes a separate snapshot.
long long wynimg_stroke_apply(void* dstp, void* basep, void* strokep,
                              double r, double g, double b, double a,
                              long long erase) {
    WynImg* d = wynimg_deref(dstp);
    WynImg* bs = wynimg_deref(basep);
    WynImg* st = wynimg_deref(strokep);
    if (!d || !bs || !st || !d->px || !bs->px || !st->px) return 0;
    if (d->w != bs->w || d->h != bs->h) return 0;
    if (d->w != st->w || d->h != st->h) return 0;

    a = clamp01d(a);
    r = clamp01d(r);
    g = clamp01d(g);
    b = clamp01d(b);

    long long n = d->w * d->h;
    for (long long i = 0; i < n; i++) {
        size_t o = (size_t)i * 4;
        double cov = clamp01d((double)st->px[o]);
        if (cov <= 0.0) {
            if (d != bs) {
                d->px[o+0] = bs->px[o+0];
                d->px[o+1] = bs->px[o+1];
                d->px[o+2] = bs->px[o+2];
                d->px[o+3] = bs->px[o+3];
            }
            continue;
        }
        double sa = a * cov;
        if (erase) {
            double keep = 1.0 - sa;
            d->px[o+0] = (float)(bs->px[o+0] * keep);
            d->px[o+1] = (float)(bs->px[o+1] * keep);
            d->px[o+2] = (float)(bs->px[o+2] * keep);
            d->px[o+3] = (float)(bs->px[o+3] * keep);
        } else {
            double da = bs->px[o+3];
            double inv = 1.0 - sa;
            d->px[o+0] = (float)(r * sa + bs->px[o+0] * inv);
            d->px[o+1] = (float)(g * sa + bs->px[o+1] * inv);
            d->px[o+2] = (float)(b * sa + bs->px[o+2] * inv);
            d->px[o+3] = (float)(sa + da * inv);
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Reads. The eyedropper and every pixel assertion in the test suite need an
// UNPREMULTIPLIED value: the stored pixel is colour*alpha, so reading a
// half-transparent red as 0.5 and calling it "the colour" is wrong.
// ---------------------------------------------------------------------------

// chan 0..3. Channels 0-2 are un-premultiplied; 3 is alpha as stored.
// Out of range reads 0.
double wynimg_pick(void* p, long long x, long long y, long long chan) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0.0;
    if (x < 0 || y < 0 || x >= im->w || y >= im->h) return 0.0;
    if (chan < 0 || chan > 3) return 0.0;
    size_t o = (size_t)(y * im->w + x) * 4;
    double al = im->px[o+3];
    if (chan == 3) return al;
    if (al <= 0.0) return 0.0;
    return clamp01d((double)im->px[o+chan] / al);
}

// ---------------------------------------------------------------------------
// Flood fill.
//
// 4-connected, seeded on the layer's OWN pixels (not the composite): filling
// must be undoable and layer-local, and a fill computed from the composite
// would key off pixels that are not in the layer it writes to.
//
// The tolerance compares un-premultiplied RGBA against the seed. An explicit
// stack, not recursion: a 12MP single-colour region would blow any real C stack
// depth at roughly one frame per pixel.
// ---------------------------------------------------------------------------

static void unpremul4(const float* px, size_t o, double* out) {
    double al = px[o+3];
    out[3] = al;
    if (al <= 0.0) { out[0] = out[1] = out[2] = 0.0; return; }
    out[0] = px[o+0] / al;
    out[1] = px[o+1] / al;
    out[2] = px[o+2] / al;
}

// Writes coverage 1.0 into `stroke` for every pixel reachable from (sx,sy)
// whose colour is within `tol` of the seed. The caller then runs
// wynimg_stroke_apply, so a bucket fill goes through the SAME compositing and
// the SAME undo record as a brush stroke - one code path, one behaviour.
//
// Returns the number of pixels selected, or -1 on a bad argument, so a fill
// that found nothing is distinguishable from a fill that was refused.
long long wynimg_fill_region(void* strokep, void* srcp,
                             long long sx, long long sy, double tol) {
    WynImg* st = wynimg_deref(strokep);
    WynImg* im = wynimg_deref(srcp);
    if (!st || !im || !st->px || !im->px) return -1;
    if (st->w != im->w || st->h != im->h) return -1;
    if (sx < 0 || sy < 0 || sx >= im->w || sy >= im->h) return -1;
    if (tol < 0.0) tol = 0.0;

    long long w = im->w, h = im->h, n = w * h;
    unsigned char* seen = (unsigned char*)calloc((size_t)n, 1);
    long long* stack = (long long*)malloc((size_t)n * sizeof(long long));
    if (!seen || !stack) { free(seen); free(stack); return -1; }

    double seed[4];
    unpremul4(im->px, (size_t)(sy * w + sx) * 4, seed);

    long long top = 0, count = 0;
    stack[top++] = sy * w + sx;
    seen[sy * w + sx] = 1;

    while (top > 0) {
        long long i = stack[--top];
        size_t o = (size_t)i * 4;
        double c[4];
        unpremul4(im->px, o, c);
        double worst = 0.0;
        for (int k = 0; k < 4; k++) {
            double dd = c[k] - seed[k];
            if (dd < 0.0) dd = -dd;
            if (dd > worst) worst = dd;
        }
        if (worst > tol) continue;

        st->px[o] = 1.0f;
        count++;

        long long x = i % w, y = i / w;
        if (x > 0     && !seen[i-1]) { seen[i-1] = 1; stack[top++] = i - 1; }
        if (x < w - 1 && !seen[i+1]) { seen[i+1] = 1; stack[top++] = i + 1; }
        if (y > 0     && !seen[i-w]) { seen[i-w] = 1; stack[top++] = i - w; }
        if (y < h - 1 && !seen[i+w]) { seen[i+w] = 1; stack[top++] = i + w; }
    }

    free(seen);
    free(stack);
    return count;
}

// ---------------------------------------------------------------------------
// Whole-buffer helpers used by the layer commands.
// ---------------------------------------------------------------------------

// Sets every pixel to coverage 0 - i.e. clears a stroke buffer for reuse
// without reallocating it. wynimg_fill(p,0,0,0,0) does the same thing; this
// name exists so the call site says what it means.
long long wynimg_clear(void* p) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0;
    memset(im->px, 0, (size_t)(im->w * im->h * 4) * sizeof(float));
    return 1;
}

// A rectangle of unpremultiplied linear colour, composited source-over. Used by
// the checkerboard-free "new layer" path and by tests that need a known block
// of pixels without driving a brush.
long long wynimg_rect(void* p, long long x0, long long y0,
                      long long w, long long h,
                      double r, double g, double b, double a) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0;
    if (w <= 0 || h <= 0) return 0;
    a = clamp01d(a);
    for (long long y = y0; y < y0 + h; y++) {
        if (y < 0 || y >= im->h) continue;
        for (long long x = x0; x < x0 + w; x++) {
            if (x < 0 || x >= im->w) continue;
            size_t o = (size_t)(y * im->w + x) * 4;
            double da = im->px[o+3];
            double inv = 1.0 - a;
            im->px[o+0] = (float)(clamp01d(r) * a + im->px[o+0] * inv);
            im->px[o+1] = (float)(clamp01d(g) * a + im->px[o+1] * inv);
            im->px[o+2] = (float)(clamp01d(b) * a + im->px[o+2] * inv);
            im->px[o+3] = (float)(a + da * inv);
        }
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Colour-picker ramps.
//
// WHY THESE ARE IN C AND NOT DRAWN WITH RECTANGLES.
//
// An SV square is a per-pixel gradient. Painting a 128x128 one with the window
// backend's Win_rect is 16,384 draw calls per frame - 1M/s at 60Hz, for a
// 128px widget. Painting it at a coarser granularity to save calls is what makes
// a picker look like a chessboard.
//
// So the picker is a wynimg buffer uploaded as a texture and blitted once, like
// the document. That reuses the path that already exists, and rebuilding it costs
// nothing per frame because the SV square depends only on the hue.
//
// The values written are LINEAR PREMULTIPLIED, like every other buffer here, so
// the existing upload converts them for display and the picker's colours match
// what the brush will actually paint. A picker filled with sRGB values would show
// a plausible-looking gradient that disagreed with every stroke.
//
// DUPLICATION IS GUARDED, NOT DENIED: hsv_rgb below is a second implementation of
// src/pixel.wyn's hsv_channel, and tests/test_paint.wyn asserts the two agree at
// sampled points, so the copy cannot drift unnoticed.
// ---------------------------------------------------------------------------

static void hsv_rgb(double h, double s, double v, double* out) {
    while (h >= 360.0) h -= 360.0;
    while (h < 0.0) h += 360.0;
    s = clamp01d(s);
    v = clamp01d(v);
    double c = v * s;
    double sector = h / 60.0;
    int seg = (int)sector;
    double f = sector - (double)seg;
    double x = c * (1.0 - f);
    double y = c * f;
    double m = v - c;
    double r = 0.0, g = 0.0, b = 0.0;
    switch (seg) {
        case 0:  r = c;   g = y;   b = 0.0; break;
        case 1:  r = x;   g = c;   b = 0.0; break;
        case 2:  r = 0.0; g = c;   b = y;   break;
        case 3:  r = 0.0; g = x;   b = c;   break;
        case 4:  r = y;   g = 0.0; b = c;   break;
        default: r = c;   g = 0.0; b = x;   break;
    }
    out[0] = m + r;
    out[1] = m + g;
    out[2] = m + b;
}

// sRGB -> linear, matching src/pixel.wyn's srgb_to_linear exactly (IEC 61966-2-1).
static double srgb_lin(double c) {
    c = clamp01d(c);
    if (c <= 0.04045) return c / 12.92;
    return pow((c + 0.055) / 1.055, 2.4);
}

// Saturation across x, value down y, at a fixed hue. Opaque.
long long wynimg_sv_square(void* p, double hue) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0;
    if (im->w < 2 || im->h < 2) return 0;
    for (long long y = 0; y < im->h; y++) {
        double v = 1.0 - (double)y / (double)(im->h - 1);
        for (long long x = 0; x < im->w; x++) {
            double s = (double)x / (double)(im->w - 1);
            double rgb[3];
            hsv_rgb(hue, s, v, rgb);
            size_t o = (size_t)(y * im->w + x) * 4;
            im->px[o+0] = (float)srgb_lin(rgb[0]);   // alpha 1 => premultiplied
            im->px[o+1] = (float)srgb_lin(rgb[1]);
            im->px[o+2] = (float)srgb_lin(rgb[2]);
            im->px[o+3] = 1.0f;
        }
    }
    return 1;
}

// Hue down y, full saturation and value. Opaque.
long long wynimg_hue_strip(void* p) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0;
    if (im->h < 2) return 0;
    for (long long y = 0; y < im->h; y++) {
        double h = 360.0 * (double)y / (double)im->h;
        double rgb[3];
        hsv_rgb(h, 1.0, 1.0, rgb);
        for (long long x = 0; x < im->w; x++) {
            size_t o = (size_t)(y * im->w + x) * 4;
            im->px[o+0] = (float)srgb_lin(rgb[0]);
            im->px[o+1] = (float)srgb_lin(rgb[1]);
            im->px[o+2] = (float)srgb_lin(rgb[2]);
            im->px[o+3] = 1.0f;
        }
    }
    return 1;
}

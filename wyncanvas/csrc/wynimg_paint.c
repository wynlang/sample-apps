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

// Quintic smootherstep, 0 at 0 and 1 at 1 with BOTH first and second
// derivatives zero at each end.
//
// WHY QUINTIC AND NOT THE CUBIC SMOOTHSTEP. A cubic has zero slope at the ends
// but a jump in its SECOND derivative there, and human vision differentiates
// twice: a curvature step reads as a thin bright/dark ring - Mach banding - at
// the inner edge of a soft brush, which is exactly the "banding" a soft brush is
// judged on. The quintic removes that discontinuity, at the cost of two extra
// multiplies per pixel. Both hit 0 and 1 exactly, so the endpoint guarantees
// below are unchanged.
static double smoother(double c) {
    return c * c * c * (c * (c * 6.0 - 15.0) + 10.0);
}

// Coverage at distance `d` for a brush of `radius` whose inner `hardness`
// fraction is solid.
//
// TWO EXACT VALUES ARE PART OF THE CONTRACT, and tests assert them: coverage is
// EXACTLY 1.0 anywhere in the solid core (so a hardness-1 dab centre is 1.0, not
// 0.9999) and EXACTLY 0.0 at or beyond `radius` (so a brush cannot tint pixels
// outside its own footprint). Everything between is the smootherstep.
//
// hardness 1 still gets a half-pixel antialiased rim, because a hard-edged disc
// in a float buffer is a staircase.
static double falloff(double d, double radius, double hardness) {
    if (radius <= 0.0) return 0.0;
    if (d >= radius) return 0.0;                      // EXACTLY 0 past the rim
    double inner = radius * clamp01d(hardness);
    if (inner > radius - 0.5) inner = radius - 0.5;   // always antialias the rim
    if (inner < 0.0) inner = 0.0;
    if (d <= inner) return 1.0;                       // EXACTLY 1 in the core
    double t = (d - inner) / (radius - inner);        // 0 at inner, 1 at rim
    return smoother(1.0 - t);
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

// ---------------------------------------------------------------------------
// THE DAB ENGINE.
//
// wynimg_stroke_seg above is the primitive: one capsule, exactly as wide as the
// radius, drawn in one pass. It is still what src/text.wyn and src/select.wyn
// want, and it is what a hard round brush degenerates to. It cannot express a
// brush, though, because a brush is a STAMP repeated along the path: a
// calligraphic nib, a scattered spray and a pressure taper are all properties of
// individual dabs, and a single swept capsule has no dabs to give them to.
//
// SPACING, AND WHY THE LEFTOVER MUST CROSS SEGMENT BOUNDARIES.
//
// Dabs are placed every `spacing * radius` pixels ALONG THE PATH, not once per
// input sample. That is the difference between a brush whose look is a property
// of the brush and one whose look is a property of the mouse's report rate: a
// 120Hz trackpad and a 8Hz remote-desktop session must paint the same line.
//
// The arc-length cursor therefore has to be carried from one segment to the
// next. Two bugs live in getting that wrong, and both are guarded by tests:
//
//   * reset the cursor at each segment and every sample position gets a dab -
//     the sample-rate dependency comes straight back, and slow drags get dense
//     dab clusters;
//   * stamp at each segment's START as well as carrying, and every interior
//     input point is stamped TWICE. With max() coverage that is invisible on a
//     round hard brush and glaring on a scattered or low-flow one, because the
//     duplicate dab draws a second scatter sample at the same arc length.
//
// So: `carry` is the distance still to travel before the next dab, it survives
// across dab_seg calls, and only wynimg_dab_begin ever stamps unconditionally.
//
// STATE LIVES IN C, on purpose. There is exactly one live stroke (src/paint.wyn
// holds one s_active), and the alternative - returning the new carry and dab
// index to Wyn so Wyn can pass them back - needs multi-value returns, which do
// not survive a Wyn module boundary here. wynimg_dab_carry/count expose the
// state read-only so a test can assert the carry is real rather than inferring
// it from pixels.
// ---------------------------------------------------------------------------

typedef struct {
    double radius;     // px, at pressure 1
    double hardness;   // 0..1
    double spacing;    // fraction of the CURRENT dab radius, > 0
    double angle;      // radians, major axis direction
    double aspect;     // minor/major, (0,1]
    double scatter;    // fraction of radius, 0 = none
    long long seed;    // scatter PRNG seed; same seed => same stroke
    long long smooth;  // 1 = Catmull-Rom the input path (OFF by default)
} Brush;

// Defaults: a round, unscattered, unsmoothed brush at 25% spacing. Chosen so
// that a caller who never touches the new knobs gets the geometry the old
// capsule kernel gave - see the test suite's existing capsule assertions.
static Brush g_brush = { 12.0, 0.6, 0.25, 0.0, 1.0, 0.0, 1, 0 };

typedef struct {
    double x, y;       // last input point
    double press;      // pressure at that point
    double carry;      // distance still to travel before the next dab
    long long index;   // dabs stamped so far; also the scatter sequence index
    // The two input points BEFORE (x,y), for Catmull-Rom. Only read when
    // g_brush.smooth is set.
    double px_, py_;   // one back
    double qx, qy;     // two back
    long long n;       // input points seen this stroke
} DabRun;

static DabRun g_run;

long long wynimg_brush_set(double radius, double hardness, double spacing,
                           double angle_deg, double aspect,
                           double scatter, long long seed) {
    if (radius < 0.0) radius = 0.0;
    g_brush.radius = radius;
    g_brush.hardness = clamp01d(hardness);
    // A spacing of 0 would be an infinite number of dabs; 4 radii apart is
    // already a dotted line, past which the value is meaningless rather than
    // interesting. Clamping (not refusing) keeps a slider from being able to
    // hang the editor.
    if (spacing < 0.01) spacing = 0.01;
    if (spacing > 4.0) spacing = 4.0;
    g_brush.spacing = spacing;
    g_brush.angle = angle_deg * 3.14159265358979323846 / 180.0;
    if (aspect < 0.05) aspect = 0.05;   // a zero-width nib paints nothing
    if (aspect > 1.0) aspect = 1.0;     // >1 is the same nib rotated 90 degrees
    g_brush.aspect = aspect;
    if (scatter < 0.0) scatter = 0.0;
    if (scatter > 4.0) scatter = 4.0;
    g_brush.scatter = scatter;
    g_brush.seed = seed;
    return 1;
}

// Input-path smoothing, OFF by default. See the INPUT-PATH SMOOTHING note
// above wynimg_dab_seg.
long long wynimg_brush_smoothing(long long on) {
    g_brush.smooth = (on != 0) ? 1 : 0;
    return 1;
}

double    wynimg_dab_carry(void) { return g_run.carry; }
long long wynimg_dab_count(void) { return g_run.index; }

// SCATTER IS A HASH, NOT A RANDOM NUMBER GENERATOR.
//
// A brush whose jitter comes from rand() cannot be tested, and worse, cannot be
// undone and redone into the same pixels: history replays the stroke, the RNG has
// moved on, and the image changes under the user. So each dab's offset is a pure
// function of (seed, dab index, axis) - the same stroke always paints the same
// pixels, two seeds paint different ones, and there is no hidden state to reset.
//
// This is splitmix64's finaliser. It is chosen for having no linear structure in
// its low bits: a cheaper hash (index * large_odd_constant) produces offsets that
// walk in a visible straight line along the stroke, which reads as a texture
// rather than as scatter.
static double hash01(long long seed, long long index, long long axis) {
    unsigned long long z = (unsigned long long)seed * 0x9E3779B97F4A7C15ULL
                         + (unsigned long long)index * 0xBF58476D1CE4E5B9ULL
                         + (unsigned long long)axis  * 0x94D049BB133111EBULL;
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    // Top 53 bits, so the result is a uniform double in [0,1) with no bias from
    // the weaker low bits.
    return (double)(z >> 11) * (1.0 / 9007199254740992.0);
}

// The offset applied to dab `index`, in px. Scatter is a fraction of the radius,
// and the offset is sampled in a DISC rather than a square: a square jitter box
// makes a wide stroke's edges visibly straight and its corners heavy.
static void scatter_offset(long long index, double* ox, double* oy) {
    *ox = 0.0;
    *oy = 0.0;
    if (g_brush.scatter <= 0.0) return;
    double ang = hash01(g_brush.seed, index, 0) * 6.283185307179586;
    // sqrt of a uniform gives a uniform AREA density; using the raw uniform
    // clusters every dab near the path centre and scatter barely reads.
    double rr = sqrt(hash01(g_brush.seed, index, 1)) * g_brush.scatter * g_brush.radius;
    *ox = cos(ang) * rr;
    *oy = sin(ang) * rr;
}

// The arc length between a dab painted at pressure `press` and the next one.
// Scaled by pressure so a tapering tail gets DENSER dabs rather than breaking
// into beads as its radius shrinks. Floored so a zero-pressure sample cannot
// make the placement loop below spin forever.
static double dab_step(double press) {
    double step = g_brush.spacing * g_brush.radius * (press > 0.0 ? press : 1.0);
    if (step < 0.05) step = 0.05;
    return step;
}

// One dab, centred at (cx,cy), radius scaled by `press`.
//
// The dab is an ELLIPSE: distance is measured after rotating into the brush's
// frame and stretching the minor axis, so `radius` is the half-length along
// `angle` and `radius*aspect` the half-width across it. A round brush is the
// aspect-1 case of the same code, which is why there is no second loop for it.
static void stamp_dab(WynImg* s, double cx, double cy, double press) {
    double rad = g_brush.radius * press;
    if (rad <= 0.0) return;
    // The bounding box uses the MAJOR axis in both directions: at any rotation
    // the ellipse fits inside that square, and a box computed from the rotated
    // extents would have to be recomputed per angle for no measurable gain on
    // brush-sized areas.
    long long xa = (long long)floor(cx - rad - 1.0);
    long long xb = (long long)ceil(cx + rad + 1.0);
    long long ya = (long long)floor(cy - rad - 1.0);
    long long yb = (long long)ceil(cy + rad + 1.0);
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > s->w - 1) xb = s->w - 1;
    if (yb > s->h - 1) yb = s->h - 1;

    double ca = cos(-g_brush.angle), sa = sin(-g_brush.angle);
    double inv_aspect = 1.0 / g_brush.aspect;

    for (long long y = ya; y <= yb; y++) {
        for (long long x = xa; x <= xb; x++) {
            // Pixel CENTRES, so an integer-coordinate dab is symmetric rather
            // than biased half a pixel up and left.
            double dx = (double)x + 0.5 - cx;
            double dy = (double)y + 0.5 - cy;
            double u = dx * ca - dy * sa;
            double v = (dx * sa + dy * ca) * inv_aspect;
            double d = sqrt(u * u + v * v);
            double c = falloff(d, rad, g_brush.hardness);
            if (c <= 0.0) continue;
            size_t o = (size_t)(y * s->w + x) * 4;
            // MAX, NEVER +=. A sum makes overlapping dabs accumulate, which is
            // the whole bug this stroke model exists to avoid: a 30%-alpha
            // stroke would go opaque wherever the pointer moved slowly, and at
            // 25% spacing every pixel is covered by four or more dabs. Tested
            // by "a 30%-alpha stroke over itself does not reach opacity".
            if (c > (double)s->px[o]) s->px[o] = (float)c;
        }
    }
}

// One dab at the path point (px,py), displaced by dab `index`'s scatter offset.
// Every stamping site goes through here so the offset cannot be applied at one
// site and forgotten at another - which would make the first dab of a scattered
// stroke the only one on the path.
static void stamp_scattered(WynImg* s, double px, double py,
                            double press, long long index) {
    double ox, oy;
    scatter_offset(index, &ox, &oy);
    stamp_dab(s, px + ox, py + oy, press);
}

// Begins a dab run: resets the arc-length cursor and stamps the first dab.
//
// The first dab is unconditional because a click with no drag must paint. Every
// LATER dab is placed by arc length only, so this is the single point in the
// engine that stamps without consulting the carry.
long long wynimg_dab_begin(void* strokep, double x, double y, double press) {
    WynImg* s = wynimg_deref(strokep);
    g_run.x = x;
    g_run.y = y;
    g_run.px_ = x;  g_run.py_ = y;
    g_run.qx = x;   g_run.qy = y;
    g_run.press = clamp01d(press);
    g_run.carry = 0.0;
    g_run.index = 0;
    g_run.n = 1;
    if (!s || !s->px) return 0;
    if (g_brush.radius <= 0.0) return 0;
    stamp_scattered(s, x, y, g_run.press, 0);
    g_run.index = 1;
    g_run.carry = dab_step(g_run.press);   // the next dab is one spacing away
    return 1;
}

// INPUT-PATH SMOOTHING (Catmull-Rom), OFF BY DEFAULT.
//
// A pointer reports a polyline. Drawn as straight chords, a fast curve shows its
// corners, which is the one artefact people call "the brush feels cheap". A
// centripetal-free uniform Catmull-Rom through the last four input points is the
// standard fix: it interpolates (the curve passes through the samples, so the
// stroke still goes where the user pointed) and needs only local history.
//
// WHY IT IS OFF BY DEFAULT, and why turning it on would be a BEHAVIOUR CHANGE
// rather than an improvement: the spline moves where dabs land. A 90-degree
// corner drawn as two segments becomes a rounded corner, and a straight line
// entered as three collinear samples stays straight only because Catmull-Rom
// happens to be linear on collinear input - a curve entered as a polyline gets
// visibly shorter chords near its ends, where the spline has no fourth point and
// must extrapolate. Every existing test in this suite states the geometry of the
// STRAIGHT interpretation: "a drag paints a continuous capsule" measures pixels
// 10..80 on the line y=16, and "a calligraphic nib is directional" measures exact
// footprint extents. Making the spline the default would move those pixels, so
// the flag exists and the default is what the tests describe. It is the caller's
// (a UI checkbox's) choice, not the kernel's.
//
// t in [0,1] between p1 and p2; p0 and p3 are the neighbours. Standard uniform
// Catmull-Rom basis.
static double catmull(double p0, double p1, double p2, double p3, double t) {
    double t2 = t * t, t3 = t2 * t;
    return 0.5 * ((2.0 * p1)
                + (-p0 + p2) * t
                + (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2
                + (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

// The point at parameter t along the current segment, honouring g_brush.smooth.
// With smoothing off this is exactly the straight chord, which is why the
// placement loop below has no second copy for the unsmoothed case.
static void path_point(double t01, double x0, double y0, double x1, double y1,
                       double* ox, double* oy) {
    if (!g_brush.smooth || g_run.n < 2) {
        *ox = x0 + (x1 - x0) * t01;
        *oy = y0 + (y1 - y0) * t01;
        return;
    }
    // p1..p2 is the current segment; p0 is the previous input point. p3 is not
    // known yet - the pointer has not reported it - so the segment is drawn with
    // p3 = p2, which makes the curve leave p2 straight. A one-segment lag that
    // waited for p3 would smooth better and put the live preview one sample
    // behind the cursor, which reads as latency; that trade is deliberately made
    // in favour of the preview.
    *ox = catmull(g_run.px_, x0, x1, x1, t01);
    *oy = catmull(g_run.py_, y0, y1, y1, t01);
}

// Walks the path from the previous input point to (x,y), stamping a dab every
// `spacing * radius` pixels of arc length and leaving the remainder in
// g_run.carry for the next call. Pressure ramps linearly along the segment.
// Returns the number of dabs stamped, which is 0 for a short segment - and that
// zero is the point: it is where the leftover is being carried instead.
long long wynimg_dab_seg(void* strokep, double x, double y, double press) {
    WynImg* s = wynimg_deref(strokep);
    if (!s || !s->px) return 0;
    if (g_brush.radius <= 0.0) return 0;

    double x0 = g_run.x, y0 = g_run.y;
    double p0 = g_run.press, p1 = clamp01d(press);
    double dx = x - x0, dy = y - y0;
    double len = sqrt(dx * dx + dy * dy);

    long long made = 0;
    if (len > 0.0) {
        double t = g_run.carry;                 // distance to the next dab
        while (t <= len) {
            double f = t / len;
            double pr = p0 + (p1 - p0) * f;
            // The chord length is used as the arc-length parameter even when
            // smoothing bends the path. The spline is at most a few percent
            // longer than its chord over one pointer sample, so the error is
            // smaller than the spacing quantisation it feeds; measuring true arc
            // length would need a per-segment integral for no visible gain.
            double cx, cy;
            path_point(f, x0, y0, x, y, &cx, &cy);
            stamp_scattered(s, cx, cy, pr, g_run.index);
            g_run.index++;
            made++;
            t += dab_step(pr);
        }
        g_run.carry = t - len;                  // THE CARRY. See the note above.
    }

    g_run.qx = g_run.px_;  g_run.qy = g_run.py_;
    g_run.px_ = x0;        g_run.py_ = y0;
    g_run.x = x;  g_run.y = y;
    g_run.press = p1;
    g_run.n++;
    return made;
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

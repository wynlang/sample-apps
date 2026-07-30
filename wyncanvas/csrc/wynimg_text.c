// wynimg_text - the text engine: font loading, metrics, layout, and glyph
// rasterisation into a COVERAGE buffer.
//
// WHY COVERAGE AND NOT COLOUR.
//
// Nothing here writes a colour. Laying out a string produces the same thing a
// brush stroke produces - a float 0..1 coverage field in the R channel of a
// wynimg buffer - and the caller then runs `wynimg_stroke_apply` to turn that
// into premultiplied linear pixels. That is not a stylistic choice: it is what
// makes text compose with the rest of the editor for free. Text gains the
// eraser (destination-out), one-entry undo, layer masks and any future selection
// without a second compositing path, because there is only one compositing path.
// A `wynimg_draw_text_in_colour` would have had to re-derive premultiply,
// linear-light and alpha semantics, and would have disagreed with the brush the
// first time one of them was fixed.
//
// GAMMA - THE THING THAT MAKES TEXT LOOK RIGHT OR WRONG.
//
// stbtt's rasteriser reports the fraction of each pixel's AREA covered by the
// glyph outline. That number is geometrically correct, but every font on your
// screen was designed and hinted against renderers that then blend it in a
// PERCEPTUAL (gamma-encoded) space. Blend the same coverage in linear light -
// which this project must, because its buffers are linear - and the identical
// outline comes out visibly THINNER and lighter: at 12px, body text looks
// washed out and stems disappear.
//
// So coverage is passed through `cov^(1/gamma)` before it is stored, and gamma
// is a PARAMETER rather than a constant:
//
//   gamma = 1.0   raw area coverage. Physically correct for linear blending,
//                 and what you want when compositing text as a mask or matte.
//   gamma = 2.2   full compensation. Approximates the weight a gamma-space
//                 renderer would give, i.e. what the type designer drew.
//   in between    partial; 1.8 is a reasonable default for dark-on-light.
//
// It is a parameter and not a constant because the right answer depends on what
// the coverage is FOR (display text vs. a stencil), and because a constant here
// would be an invisible fudge factor in a pixel pipeline that otherwise has
// none. `wynimg_text_cov_sum` at gamma 1.0 is a real area, which is what the
// tests measure against.
//
// HANDLES, NOT POINTERS - for fonts too.
//
// A font handle is generation-tagged exactly like a buffer handle, and for the
// same reason: Wyn cannot be stopped from holding a stale one. The extra hazard
// here is that `stbtt_fontinfo` keeps RAW POINTERS INTO the font file bytes, so
// the file buffer must outlive the font, and a stale handle that resolved to a
// freed slot would read a freed 700KB mapping and render garbled glyphs rather
// than crash - the worst possible failure, because it looks like a font bug.
// Freeing bumps the slot generation, so every later use resolves to NULL and
// takes the same no-op path as a null argument.
#include "wynimg.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Vendored (csrc/stb_truetype.h), and compiled with its own diagnostics
// suppressed: STBTT_STATIC makes every entry point static, so the ~28 helpers
// this file does not call each trip -Wunused-function. Those warnings are about
// upstream's code, and letting them through would train us to ignore a
// non-empty build log. STBTT_STATIC also keeps these symbols out of the link,
// which matters because libgui.a contains its own copy of the same header.
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "stb_truetype.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#pragma clang diagnostic pop

// Prototypes live here rather than in csrc/wynimg.h because this translation
// unit is self-contained: nothing else in the shim calls into the text engine,
// and the FFI declarations that matter are the ones in src/text.wyn.
void*       wynimg_font_load(const char* path);
void*       wynimg_font_default(void);
const char* wynimg_font_default_path(void);
long long   wynimg_font_alive(void* fp);
void        wynimg_font_free(void* fp);

double wynimg_font_ascent(void* fp, double px);
double wynimg_font_descent(void* fp, double px);
double wynimg_font_line_gap(void* fp, double px);
double wynimg_font_line_height(void* fp, double px);

double    wynimg_text_width(void* fp, const char* s, double px);
long long wynimg_text_lines(void* fp, const char* s, double px, double wrap_w);
double    wynimg_text_box_w(void* fp, const char* s, double px, double wrap_w);
double    wynimg_text_box_h(void* fp, const char* s, double px,
                            double line_h, double wrap_w);

long long wynimg_text_layout(void* covp, void* fp, const char* s,
                             double x, double y, double px,
                             double line_h, long long align,
                             double wrap_w, double gamma);

double    wynimg_text_cov_sum(void* p);
double    wynimg_text_cov_max(void* p);
double    wynimg_text_cov_centroid_x(void* p);
double    wynimg_text_cov_centroid_y(void* p);
long long wynimg_text_cov_peak_x(void* p);
long long wynimg_text_cov_peak_y(void* p);

// ---------------------------------------------------------------------------
// Font handle table.
//
// Same encoding as csrc/wynimg_buf.c: (slot+1) in the low bits, a generation
// counter above it, both clear of the sign bit so the handle survives Wyn's
// signed `int` FFI round trip. A fixed 64 slots rather than a growable table
// because fonts are units, not pixels - a document with 64 live typefaces is
// already implausible, and refusing the 65th is better than an unbounded table.
// ---------------------------------------------------------------------------

#define FONT_SLOTS      64
#define FONT_SLOT_BITS  8
#define FONT_SLOT_MASK  ((1LL << FONT_SLOT_BITS) - 1)
#define FONT_GEN_MASK   ((1LL << 39) - 1)

// Refusals, not clamps, for the two dimensions that can turn a typo into a
// multi-gigabyte allocation. 4000px is far past any real type size; 4096 lines
// is past any real text layer.
#define TEXT_MAX_PX     4000.0
#define TEXT_MAX_LINES  4096

typedef struct {
    stbtt_fontinfo info;
    unsigned char* data;      // the whole font file, owned. stbtt keeps
                              // pointers INTO this, so it must not be freed
                              // until the font is.
    int  ascent, descent, line_gap;   // font design units
    long long gen;
    int  used;
} TextFont;

static TextFont  g_fonts[FONT_SLOTS];
static long long g_font_gen = 1;

static void* font_encode(long long slot, long long gen) {
    return (void*)(((unsigned long long)(gen & FONT_GEN_MASK) << FONT_SLOT_BITS)
                   | (unsigned long long)((slot + 1) & FONT_SLOT_MASK));
}

// Resolves a handle to a live font, or NULL if it is null, out of range,
// already freed, or from an earlier generation of its slot.
static TextFont* font_deref(void* fp) {
    long long h = (long long)fp;
    if (h == 0) return NULL;
    long long idx = (h & FONT_SLOT_MASK) - 1;
    if (idx < 0 || idx >= FONT_SLOTS) return NULL;
    TextFont* f = &g_fonts[idx];
    if (!f->used) return NULL;
    if ((f->gen & FONT_GEN_MASK) != ((h >> FONT_SLOT_BITS) & FONT_GEN_MASK))
        return NULL;
    return f;
}

// `px` is the ASCENT-TO-DESCENT height in pixels, matching
// stbtt_ScaleForPixelHeight and repos/gui's Win_font. It is NOT the em size, so
// a 40px request yields a cap height near 29px in Arial - the same convention
// the rest of this workspace already uses, and changing it here would make the
// editor's text a different size from the toolkit's.
static float scale_for(const TextFont* f, double px) {
    if (px <= 0.0 || px > TEXT_MAX_PX) return 0.0f;
    return stbtt_ScaleForPixelHeight(&f->info, (float)px);
}

void* wynimg_font_load(const char* path) {
    if (!path) return NULL;
    FILE* fp = fopen(path, "rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    long size = ftell(fp);
    if (size <= 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }

    unsigned char* data = (unsigned char*)malloc((size_t)size);
    if (!data) { fclose(fp); return NULL; }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    int idx = -1;
    for (int i = 0; i < FONT_SLOTS; i++) if (!g_fonts[i].used) { idx = i; break; }
    if (idx < 0) { free(data); return NULL; }

    TextFont* f = &g_fonts[idx];
    memset(f, 0, sizeof(*f));
    f->data = data;

    // Offset-for-index 0 also handles a .ttc collection, which is what most of
    // macOS's system fonts are.
    int off = stbtt_GetFontOffsetForIndex(data, 0);
    if (off < 0 || !stbtt_InitFont(&f->info, f->data, off)) {
        free(data);
        f->data = NULL;
        return NULL;
    }
    stbtt_GetFontVMetrics(&f->info, &f->ascent, &f->descent, &f->line_gap);
    // A font whose ascent and descent are equal has no usable vertical metrics;
    // every line would land on top of the last one.
    if (f->ascent - f->descent <= 0) {
        free(data);
        f->data = NULL;
        return NULL;
    }
    f->gen = g_font_gen++;
    f->used = 1;
    return font_encode(idx, f->gen);
}

// Remembers which candidate wynimg_font_default actually opened, so a test can
// print it. Static storage rather than a returned allocation because the Wyn
// FFI has no way to free a string it receives.
static char g_default_path[512];

// A usable system font, or 0 if the machine has none.
//
// This exists so the text tests need no committed TTF asset: a font in the repo
// would add hundreds of KB to every checkout, and a licence question, to test
// code that does not care which typeface it measures. The caller is expected to
// SKIP rather than fail when this returns 0 - a machine with no fonts is a fact
// about the machine, not a defect in the layout engine.
void* wynimg_font_default(void) {
    // Ordered most-likely-first per platform. Each is a real default on its
    // platform, and the .ttf entries come before the .ttc ones because a
    // single-face file is the simpler thing to get right.
    static const char* candidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",        // macOS
        "/System/Library/Fonts/Supplemental/Verdana.ttf",      // macOS
        "/System/Library/Fonts/Geneva.ttf",                    // macOS
        "/System/Library/Fonts/Monaco.ttf",                    // macOS
        "/System/Library/Fonts/Helvetica.ttc",                 // macOS
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",     // Debian/Ubuntu
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",              // Fedora
        "/usr/share/fonts/TTF/DejaVuSans.ttf",                 // Arch
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
        "/usr/share/fonts/noto/NotoSans-Regular.ttf",
        "C:\\Windows\\Fonts\\arial.ttf",                       // Windows
        "C:\\Windows\\Fonts\\segoeui.ttf",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        void* h = wynimg_font_load(candidates[i]);
        if (h) {
            snprintf(g_default_path, sizeof(g_default_path), "%s", candidates[i]);
            return h;
        }
    }
    g_default_path[0] = '\0';
    return NULL;
}

// The path wynimg_font_default last opened, or "" if it never succeeded.
const char* wynimg_font_default_path(void) { return g_default_path; }

long long wynimg_font_alive(void* fp) { return font_deref(fp) ? 1 : 0; }

void wynimg_font_free(void* fp) {
    long long h = (long long)fp;
    if (h == 0) return;
    long long idx = (h & FONT_SLOT_MASK) - 1;
    if (idx < 0 || idx >= FONT_SLOTS) return;
    TextFont* f = &g_fonts[idx];
    if (!f->used) return;                                   // double free: no-op
    if ((f->gen & FONT_GEN_MASK) != ((h >> FONT_SLOT_BITS) & FONT_GEN_MASK))
        return;                                             // stale generation
    free(f->data);                 // must outlive nothing: stbtt_fontinfo dies here
    f->data = NULL;
    f->used = 0;
    f->gen = g_font_gen++;         // invalidate every outstanding copy of `fp`
}

// ---------------------------------------------------------------------------
// Metrics. All four are scaled to the requested pixel size, because a caller
// that had to scale design units itself would need to know which scale
// convention this file picked.
// ---------------------------------------------------------------------------

double wynimg_font_ascent(void* fp, double px) {
    TextFont* f = font_deref(fp);
    if (!f) return 0.0;
    return (double)f->ascent * (double)scale_for(f, px);
}

double wynimg_font_descent(void* fp, double px) {
    TextFont* f = font_deref(fp);
    if (!f) return 0.0;
    return (double)f->descent * (double)scale_for(f, px);   // negative
}

double wynimg_font_line_gap(void* fp, double px) {
    TextFont* f = font_deref(fp);
    if (!f) return 0.0;
    return (double)f->line_gap * (double)scale_for(f, px);
}

// The font's own default baseline-to-baseline distance: ascent - descent + gap.
// A line-height multiplier of 1.0 means exactly this.
double wynimg_font_line_height(void* fp, double px) {
    TextFont* f = font_deref(fp);
    if (!f) return 0.0;
    return (double)(f->ascent - f->descent + f->line_gap) * (double)scale_for(f, px);
}

// ---------------------------------------------------------------------------
// UTF-8 and run measurement.
// ---------------------------------------------------------------------------

// Decodes one codepoint and advances *pp. Returns 0 at the NUL terminator.
// Malformed bytes yield U+FFFD and consume one byte, so a bad string renders as
// visible replacement boxes and terminates, rather than looping or walking off
// the end.
static int utf8_next(const char** pp) {
    const unsigned char* p = (const unsigned char*)*pp;
    unsigned c = p[0];
    if (c == 0) return 0;
    int n;
    unsigned cp;
    if (c < 0x80)             { *pp = (const char*)(p + 1); return (int)c; }
    else if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1Fu; }
    else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0Fu; }
    else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07u; }
    else                      { *pp = (const char*)(p + 1); return 0xFFFD; }
    for (int i = 1; i <= n; i++) {
        if ((p[i] & 0xC0) != 0x80) { *pp = (const char*)(p + 1); return 0xFFFD; }
        cp = (cp << 6) | (unsigned)(p[i] & 0x3F);
    }
    *pp = (const char*)(p + n + 1);
    return (int)cp;
}

// Advance width of `len` bytes starting at `s`, in pixels, WITH kerning between
// adjacent pairs.
//
// Accumulated as a double and never rounded per glyph. Rounding each advance to
// an integer - which is what a bitmap-era renderer does - makes "MM" wider or
// narrower than 2 x "M" by up to a pixel, and makes a long line drift; sub-pixel
// positioning is the whole reason this returns a double.
static double run_width(const stbtt_fontinfo* fi, const char* s, int len,
                        float scale) {
    double w = 0.0;
    const char* p = s;
    const char* end = s + len;
    int prev = 0;
    while (p < end) {
        const char* q = p;
        int cp = utf8_next(&q);
        if (cp == 0 || q > end) break;
        if (prev) w += (double)stbtt_GetCodepointKernAdvance(fi, prev, cp) * scale;
        int adv, lsb;
        stbtt_GetCodepointHMetrics(fi, cp, &adv, &lsb);
        w += (double)adv * scale;
        prev = cp;
        p = q;
    }
    return w;
}

// ---------------------------------------------------------------------------
// Line breaking.
// ---------------------------------------------------------------------------

typedef struct {
    const char* s;
    int         len;
    double      w;      // advance width in pixels
} TextLine;

// Splits `s` into lines at explicit '\n', then greedily word-wraps each to
// `wrap_w` if that is positive. Returns the line count, or -1 if the text needs
// more than TEXT_MAX_LINES lines.
//
// An empty hard line ("a\n\nb") is kept as a zero-width line, because dropping
// it would silently close up the blank line the author typed.
//
// LIMITATION, deliberate and documented rather than half-implemented: a single
// word longer than `wrap_w` is placed on its own line and allowed to overflow.
// Breaking mid-word needs hyphenation or grapheme-cluster rules to not be
// actively wrong, and a wrong break is worse than an overflow you can see.
static int layout_lines(const stbtt_fontinfo* fi, const char* s, float scale,
                        double wrap_w, TextLine* out, int maxl) {
    int n = 0;
    const char* seg = s;
    for (;;) {
        const char* nl = strchr(seg, '\n');
        const char* segend = nl ? nl : seg + strlen(seg);

        if (wrap_w <= 0.0) {
            if (n >= maxl) return -1;
            out[n].s = seg;
            out[n].len = (int)(segend - seg);
            out[n].w = run_width(fi, seg, out[n].len, scale);
            n++;
        } else {
            const char* ls = seg;            // start of the line being built
            const char* fit = NULL;          // end of the content known to fit
            const char* p = seg;
            while (p < segend) {
                while (p < segend && *p == ' ') p++;      // gap before the word
                const char* wstart = p;
                while (p < segend && *p != ' ') {
                    const char* q = p;
                    utf8_next(&q);
                    p = q;
                }
                if (p == wstart) break;                   // only trailing spaces
                double cand = run_width(fi, ls, (int)(p - ls), scale);
                if (cand > wrap_w && fit) {
                    if (n >= maxl) return -1;
                    out[n].s = ls;
                    out[n].len = (int)(fit - ls);
                    out[n].w = run_width(fi, ls, out[n].len, scale);
                    n++;
                    ls = wstart;                          // the word starts the next line
                }
                fit = p;
            }
            if (n >= maxl) return -1;
            out[n].s = ls;
            out[n].len = fit ? (int)(fit - ls) : 0;
            out[n].w = run_width(fi, ls, out[n].len, scale);
            n++;
        }

        if (!nl) break;
        seg = nl + 1;
    }
    return n;
}

// ---------------------------------------------------------------------------
// Measurement entry points.
// ---------------------------------------------------------------------------

// The widest line's advance width, ignoring wrapping. "" is 0.
double wynimg_text_width(void* fp, const char* s, double px) {
    TextFont* f = font_deref(fp);
    if (!f || !s) return 0.0;
    float scale = scale_for(f, px);
    if (scale <= 0.0f) return 0.0;

    double worst = 0.0;
    const char* seg = s;
    for (;;) {
        const char* nl = strchr(seg, '\n');
        const char* segend = nl ? nl : seg + strlen(seg);
        double w = run_width(&f->info, seg, (int)(segend - seg), scale);
        if (w > worst) worst = w;
        if (!nl) break;
        seg = nl + 1;
    }
    return worst;
}

// Allocates the line table on the heap: TEXT_MAX_LINES TextLines is ~96KB,
// well past what belongs on a stack frame shared with a recursive rasteriser.
static TextLine* lines_alloc(void) {
    return (TextLine*)malloc((size_t)TEXT_MAX_LINES * sizeof(TextLine));
}

long long wynimg_text_lines(void* fp, const char* s, double px, double wrap_w) {
    TextFont* f = font_deref(fp);
    if (!f || !s) return 0;
    float scale = scale_for(f, px);
    if (scale <= 0.0f) return 0;
    TextLine* ls = lines_alloc();
    if (!ls) return 0;
    int n = layout_lines(&f->info, s, scale, wrap_w, ls, TEXT_MAX_LINES);
    free(ls);
    return n < 0 ? 0 : n;
}

// The layout box width: `wrap_w` when wrapping, else the widest laid-out line.
// This is also the width alignment is measured against, which is why a caller
// that wants centred text in a known box passes that box as wrap_w.
double wynimg_text_box_w(void* fp, const char* s, double px, double wrap_w) {
    if (wrap_w > 0.0) return wrap_w;
    return wynimg_text_width(fp, s, px);
}

double wynimg_text_box_h(void* fp, const char* s, double px,
                         double line_h, double wrap_w) {
    long long n = wynimg_text_lines(fp, s, px, wrap_w);
    if (n <= 0) return 0.0;
    if (line_h <= 0.0) line_h = 1.0;
    double lh = wynimg_font_line_height(fp, px) * line_h;
    // The last line contributes its ink height, not a full line advance, so a
    // one-line box is not padded by the descender gap of a line that is not
    // there.
    double asc = wynimg_font_ascent(fp, px);
    double desc = wynimg_font_descent(fp, px);
    return (double)(n - 1) * lh + (asc - desc);
}

// ---------------------------------------------------------------------------
// Rasterisation.
// ---------------------------------------------------------------------------

// Coverage is combined with max(), exactly like csrc/wynimg_paint.c's stroke
// accumulator, and for the same reason: adjacent glyphs whose antialiased edges
// overlap by a fraction of a pixel must saturate, not add. Summing there makes
// tight letter pairs grow a dark seam at their join.
static void cov_max_into(WynImg* im, long long x, long long y, double c) {
    if (x < 0 || y < 0 || x >= im->w || y >= im->h) return;
    size_t o = (size_t)(y * im->w + x) * 4;
    if (c > (double)im->px[o]) im->px[o] = (float)c;
}

// See the gamma note at the top of this file. gamma <= 0 or == 1 is identity;
// anything else raises coverage to the 1/gamma power, which BRIGHTENS (thickens)
// because coverage is in [0,1].
static double gamma_apply(double c, double gamma) {
    if (c <= 0.0) return 0.0;
    if (c >= 1.0) return 1.0;
    if (gamma <= 0.0 || gamma == 1.0) return c;
    if (gamma > 8.0) gamma = 8.0;
    return pow(c, 1.0 / gamma);
}

// Lays `s` out and accumulates its coverage into `covp`'s R channel.
//
// (x, y) is the TOP-LEFT of the layout box, not the baseline: a layout engine
// knows where a paragraph starts, and making callers add the ascent themselves
// is how text ends up one line too high in half the call sites. The baseline of
// line i is y + ascent + i * line_height.
//
// `align`: 0 left, 1 centre, 2 right - measured against the box width, which is
// `wrap_w` if positive and otherwise the widest line.
// `line_h`: multiplier on the font's own line height; <= 0 means 1.0.
// `gamma`: coverage ramp, see the note at the top of the file.
//
// Returns 1 on success, 0 if refused (dead handle, bad size, unrepresentable
// line count, or out of memory). Coverage is ADDED to whatever is already in the
// buffer via max(), so two calls can build one text block.
long long wynimg_text_layout(void* covp, void* fp, const char* s,
                             double x, double y, double px,
                             double line_h, long long align,
                             double wrap_w, double gamma) {
    WynImg* im = wynimg_deref(covp);
    TextFont* f = font_deref(fp);
    if (!im || !im->px || !f || !s) return 0;
    float scale = scale_for(f, px);
    if (scale <= 0.0f) return 0;
    if (line_h <= 0.0) line_h = 1.0;

    TextLine* ls = lines_alloc();
    if (!ls) return 0;
    int n = layout_lines(&f->info, s, scale, wrap_w, ls, TEXT_MAX_LINES);
    if (n < 0) { free(ls); return 0; }

    double box_w = wrap_w;
    if (box_w <= 0.0) {
        box_w = 0.0;
        for (int i = 0; i < n; i++) if (ls[i].w > box_w) box_w = ls[i].w;
    }

    double asc = (double)f->ascent * scale;
    double lh = (double)(f->ascent - f->descent + f->line_gap) * scale * line_h;

    // One scratch bitmap grown to the largest glyph seen, rather than a
    // malloc/free per glyph: a 2000-character paragraph is 2000 allocations
    // otherwise, and glyph sizes cluster tightly.
    unsigned char* scratch = NULL;
    size_t scratch_cap = 0;

    for (int i = 0; i < n; i++) {
        double off = 0.0;
        if (align == 1) off = (box_w - ls[i].w) * 0.5;
        else if (align == 2) off = box_w - ls[i].w;

        double pen_x = x + off;
        double base_y = y + asc + (double)i * lh;

        const char* p = ls[i].s;
        const char* end = ls[i].s + ls[i].len;
        int prev = 0;
        while (p < end) {
            const char* q = p;
            int cp = utf8_next(&q);
            if (cp == 0 || q > end) break;
            p = q;
            if (prev) {
                pen_x += (double)stbtt_GetCodepointKernAdvance(&f->info, prev, cp)
                         * scale;
            }
            prev = cp;

            // SUB-PIXEL POSITIONING. The fractional part of the pen position is
            // handed to stbtt as a shift so the outline is sampled at its true
            // position. Rounding the pen to an integer instead is what makes
            // rendered text look unevenly spaced at small sizes, and makes the
            // same string at two positions differ in width.
            double fx = floor(pen_x), fy = floor(base_y);
            float sx = (float)(pen_x - fx);
            float sy = (float)(base_y - fy);

            int gx0, gy0, gx1, gy1;
            stbtt_GetCodepointBitmapBoxSubpixel(&f->info, cp, scale, scale,
                                                sx, sy, &gx0, &gy0, &gx1, &gy1);
            int gw = gx1 - gx0, gh = gy1 - gy0;
            if (gw > 0 && gh > 0) {
                size_t need = (size_t)gw * (size_t)gh;
                if (need > scratch_cap) {
                    unsigned char* ns = (unsigned char*)realloc(scratch, need);
                    if (!ns) { free(scratch); free(ls); return 0; }
                    scratch = ns;
                    scratch_cap = need;
                }
                stbtt_MakeCodepointBitmapSubpixel(&f->info, scratch, gw, gh, gw,
                                                  scale, scale, sx, sy, cp);
                long long ox = (long long)fx + gx0;
                long long oy = (long long)fy + gy0;
                for (int gy = 0; gy < gh; gy++) {
                    for (int gx = 0; gx < gw; gx++) {
                        unsigned char v = scratch[(size_t)gy * (size_t)gw + (size_t)gx];
                        if (!v) continue;
                        cov_max_into(im, ox + gx, oy + gy,
                                     gamma_apply((double)v / 255.0, gamma));
                    }
                }
            }

            int adv, lsb;
            stbtt_GetCodepointHMetrics(&f->info, cp, &adv, &lsb);
            pen_x += (double)adv * scale;
        }
    }

    free(scratch);
    free(ls);
    return 1;
}

// ---------------------------------------------------------------------------
// Coverage statistics.
//
// These live in C for the same reason the pixel loops do: a Wyn-side scan of a
// 200x120 coverage buffer is 24,000 FFI calls, and the project's rule is that
// bulk pixels never cross the boundary. They are named wynimg_text_cov_* rather
// than wynimg_cov_* so they cannot collide with a general coverage/selection
// API added elsewhere in the shim.
// ---------------------------------------------------------------------------

double wynimg_text_cov_sum(void* p) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0.0;
    double s = 0.0;
    long long n = im->w * im->h;
    for (long long i = 0; i < n; i++) s += (double)im->px[i * 4];
    return s;
}

double wynimg_text_cov_max(void* p) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0.0;
    double m = 0.0;
    long long n = im->w * im->h;
    for (long long i = 0; i < n; i++) {
        double v = (double)im->px[i * 4];
        if (v > m) m = v;
    }
    return m;
}

// Coverage-weighted mean position of the ink. -1.0 when there is no ink, which
// is an impossible coordinate, so "no ink" can never be mistaken for "ink at
// the origin".
static double cov_centroid(WynImg* im, int axis) {
    if (!im || !im->px) return -1.0;
    double acc = 0.0, tot = 0.0;
    for (long long y = 0; y < im->h; y++) {
        for (long long x = 0; x < im->w; x++) {
            double v = (double)im->px[(size_t)(y * im->w + x) * 4];
            if (v <= 0.0) continue;
            tot += v;
            acc += v * (double)(axis == 0 ? x : y);
        }
    }
    if (tot <= 0.0) return -1.0;
    return acc / tot;
}

double wynimg_text_cov_centroid_x(void* p) { return cov_centroid(wynimg_deref(p), 0); }
double wynimg_text_cov_centroid_y(void* p) { return cov_centroid(wynimg_deref(p), 1); }

// Coordinates of the highest-coverage pixel, or -1 if there is no ink. A test
// that wants to assert a drawn glyph's COLOUR needs a pixel it knows is fully
// inked; hard-coding one couples the test to a particular typeface's stem
// position.
static long long cov_peak(WynImg* im, int axis) {
    if (!im || !im->px) return -1;
    double best = 0.0;
    long long bx = -1, by = -1;
    for (long long y = 0; y < im->h; y++) {
        for (long long x = 0; x < im->w; x++) {
            double v = (double)im->px[(size_t)(y * im->w + x) * 4];
            if (v > best) { best = v; bx = x; by = y; }
        }
    }
    if (bx < 0) return -1;
    return axis == 0 ? bx : by;
}

long long wynimg_text_cov_peak_x(void* p) { return cov_peak(wynimg_deref(p), 0); }
long long wynimg_text_cov_peak_y(void* p) { return cov_peak(wynimg_deref(p), 1); }

#include "wynimg.h"
#include <stdlib.h>

// ---------------------------------------------------------------------------
// Handle table.
//
// Every buffer lives in a slot; the handle Wyn holds is (slot+1) in the low
// bits and a generation counter in the high bits. Freeing bumps the slot's
// generation, so a stale handle no longer matches and resolves to NULL.
//
// This exists because Wyn cannot be stopped from holding a dangling handle:
// `wynimg_free(p)` has no way to null the caller's copy, and src/layer.wyn
// keeps buffer/mask handles in parallel arrays that happily outlive the
// buffers. With raw pointers, `layer.set_mask(t, m); wynimg_free(m); compose()`
// is a heap-use-after-free read inside wynimg_composite -- confirmed under
// ASan. With handles it is a no-op mask.
//
// 24 bits of slot index (16.7M live buffers) and 39 bits of generation, both
// well clear of the sign bit so the handle survives Wyn's signed `int` FFI
// round trip.
// ---------------------------------------------------------------------------

#define WYNIMG_SLOT_BITS  24
#define WYNIMG_SLOT_MASK  ((1LL << WYNIMG_SLOT_BITS) - 1)
#define WYNIMG_MAX_SLOTS  WYNIMG_SLOT_MASK
#define WYNIMG_GEN_MASK   ((1LL << 39) - 1)

typedef struct {
    WynImg    img;
    long long gen;      // bumped on free; even/odd carries no meaning
    int       live;
} WynSlot;

// THE SLOTS THEMSELVES MUST NEVER MOVE.
//
// This used to be a flat `WynSlot*` grown with realloc. That made
// wynimg_deref's return value a pointer INTO the growable array, so any
// wynimg_new that happened to trigger the realloc invalidated every WynImg*
// a caller was still holding. The window is one line wide and entirely
// ordinary:
//
//     WynImg* s = wynimg_deref(p);      // interior pointer into g_slots
//     void*   h = wynimg_new(s->w, s->h);   // 65th allocation -> realloc
//     memcpy(d->px, s->px, ...);        // reads through the freed old array
//
// which is exactly what csrc/wynimg_paint.c's wynimg_clone does. Confirmed
// under ASan: "heap-use-after-free ... READ of size 8 ... in wynimg_clone",
// freed by realloc in wynimg_new, after filling the table to its initial
// 64-slot capacity. It reproduced at -O2 with no invalid input at all - a
// 65th live buffer is all it takes - and it would have been a silent wrong-
// pixels clone rather than a crash, because the freed region is usually still
// mapped.
//
// So the growable array now holds POINTERS to individually-allocated slots.
// The pointer array still moves; the WynSlot objects, which are what handles
// resolve into, never do. Growth therefore cannot invalidate an outstanding
// WynImg*, which is the invariant every "deref then call" site in the shim
// already assumed.
static WynSlot** g_slots = NULL;
static long long g_cap = 0;
static long long g_len = 0;
static long long g_free_head = -1;   // -1, else index of a reusable slot
static long long g_next_gen = 1;

// Freed slots form a singly linked list threaded through img.w, which is
// meaningless while the slot is dead.
static long long slot_next_free(const WynSlot* s) { return s->img.w; }
static void      slot_set_next_free(WynSlot* s, long long n) { s->img.w = n; }

static void* handle_encode(long long slot, long long gen) {
    // slot+1 so that slot 0 never encodes to handle 0 (0 is the null handle).
    return (void*)(((unsigned long long)(gen & WYNIMG_GEN_MASK) << WYNIMG_SLOT_BITS)
                   | (unsigned long long)((slot + 1) & WYNIMG_SLOT_MASK));
}

// Resolves a handle to its live buffer, or NULL if it is null, malformed,
// out of range, already freed, or from an earlier generation of its slot.
static WynImg* handle_resolve(void* p) {
    long long h = (long long)p;
    if (h == 0) return NULL;
    long long idx = (h & WYNIMG_SLOT_MASK) - 1;
    if (idx < 0 || idx >= g_len) return NULL;
    WynSlot* s = g_slots[idx];
    if (!s->live) return NULL;
    if (((s->gen) & WYNIMG_GEN_MASK) != ((h >> WYNIMG_SLOT_BITS) & WYNIMG_GEN_MASK))
        return NULL;
    return &s->img;
}

static long long slot_alloc(void) {
    if (g_free_head >= 0) {
        long long idx = g_free_head;
        g_free_head = slot_next_free(g_slots[idx]);
        return idx;
    }
    if (g_len == g_cap) {
        long long ncap = g_cap ? g_cap * 2 : 64;
        if (ncap > WYNIMG_MAX_SLOTS) ncap = WYNIMG_MAX_SLOTS;
        if (ncap == g_cap) return -1;                      // slot table full
        // Only the pointer array is reallocated; the slots it points at stay
        // put. See the note on g_slots.
        WynSlot** ns = (WynSlot**)realloc(g_slots, (size_t)ncap * sizeof(WynSlot*));
        if (!ns) return -1;
        g_slots = ns;
        g_cap = ncap;
    }
    WynSlot* s = (WynSlot*)calloc(1, sizeof(WynSlot));
    if (!s) return -1;
    g_slots[g_len] = s;
    return g_len++;
}

void* wynimg_new(long long w, long long h) {
    if (w <= 0 || h <= 0) return NULL;
    // Overflow guard. Without it `(size_t)w * (size_t)h * 4` wraps: e.g.
    // w=2305843009213693960, h=2 wraps to 64, so calloc hands back 256 bytes
    // while im->w keeps the huge value. wynimg_count() then also overflows
    // (signed UB), which defeats the bounds checks in get/set and lets
    // wynimg_fill write far past the end of the allocation.
    if (w > WYNIMG_MAX_PIXELS || h > WYNIMG_MAX_PIXELS) return NULL;
    if (w * h > WYNIMG_MAX_PIXELS) return NULL;   // safe: both factors bounded

    float* px = (float*)calloc((size_t)(w * h * 4), sizeof(float));
    if (!px) return NULL;

    long long idx = slot_alloc();
    if (idx < 0) { free(px); return NULL; }

    WynSlot* s = g_slots[idx];
    s->img.w = w;
    s->img.h = h;
    s->img.px = px;
    s->gen = g_next_gen++;
    s->live = 1;
    return handle_encode(idx, s->gen);
}

void wynimg_free(void* p) {
    long long h = (long long)p;
    if (h == 0) return;
    long long idx = (h & WYNIMG_SLOT_MASK) - 1;
    if (idx < 0 || idx >= g_len) return;
    WynSlot* s = g_slots[idx];
    if (!s->live) return;                                  // double free: no-op
    if (((s->gen) & WYNIMG_GEN_MASK) != ((h >> WYNIMG_SLOT_BITS) & WYNIMG_GEN_MASK))
        return;                                            // stale generation
    free(s->img.px);
    s->img.px = NULL;
    s->img.h = 0;
    s->live = 0;
    s->gen = g_next_gen++;      // invalidate every outstanding copy of `p`
    slot_set_next_free(s, g_free_head);
    // Note: the slot object itself is deliberately NOT freed. Keeping it alive
    // is what lets a stale WynImg* stay addressable (and dead) instead of
    // dangling, and slots are reused from the free list anyway.
    g_free_head = idx;
}

long long wynimg_alive(void* p) { return handle_resolve(p) ? 1 : 0; }

// Internal accessor for the other translation units, which receive handles
// across the FFI boundary and must resolve them exactly the way this one does.
WynImg* wynimg_deref(void* p) { return handle_resolve(p); }

long long wynimg_width(void* p)  { WynImg* im = handle_resolve(p); return im ? im->w : 0; }
long long wynimg_height(void* p) { WynImg* im = handle_resolve(p); return im ? im->h : 0; }

// Always exact: wynimg_new caps w*h at WYNIMG_MAX_PIXELS, so w*h*4 is at most
// 2^31 and cannot overflow long long.
static long long wynimg_count(const WynImg* im) {
    return im->w * im->h * 4;
}

double wynimg_get(void* p, long long idx) {
    WynImg* im = handle_resolve(p);
    if (!im || !im->px || idx < 0 || idx >= wynimg_count(im)) return 0.0;
    return (double)im->px[idx];
}

void wynimg_set(void* p, long long idx, double v) {
    WynImg* im = handle_resolve(p);
    if (!im || !im->px || idx < 0 || idx >= wynimg_count(im)) return;
    im->px[idx] = (float)v;
}

double wynimg_max_diff(void* ap, void* bp) {
    WynImg* a = handle_resolve(ap);
    WynImg* b = handle_resolve(bp);
    if (!a || !b || !a->px || !b->px) return 2.0;
    if (a->w != b->w || a->h != b->h) return 2.0;
    double worst = 0.0;
    long long n = a->w * a->h * 4;
    for (long long i = 0; i < n; i++) {
        double d = (double)a->px[i] - (double)b->px[i];
        // A NaN in EITHER buffer is incomparable, so it takes the same
        // impossible-2.0 exit as a dimension mismatch, for the same reason: the
        // whole contract of this function is that a difference it cannot measure
        // must never masquerade as a pass.
        //
        // The bug this replaces was the worst kind. `d` is NaN whenever either
        // side is NaN; `if (d < 0.0)` and `if (d > worst)` are BOTH false for
        // NaN, so the NaN channel was skipped entirely and the function returned
        // the difference over the remaining channels -- 0.0 for two otherwise
        // identical buffers. Measured: a 4x4 buffer with px[0] = NaN compared
        // against a clean copy returned 0.000000. Every golden test in
        // tests/test_golden.wyn and the round-trip check in tests/test_e2e.wyn
        // asserts `max_diff < 0.006`, so a document with a NaN hole in it PASSED
        // the comparison that exists to catch exactly that.
        if (d != d) return 2.0;
        if (d < 0.0) d = -d;
        if (d > worst) worst = d;
    }
    return worst;
}

void wynimg_fill(void* p, double r, double g, double b, double a) {
    WynImg* im = handle_resolve(p);
    if (!im || !im->px) return;
    long long n = im->w * im->h;
    for (long long i = 0; i < n; i++) {
        im->px[i*4+0] = (float)r;
        im->px[i*4+1] = (float)g;
        im->px[i*4+2] = (float)b;
        im->px[i*4+3] = (float)a;
    }
}

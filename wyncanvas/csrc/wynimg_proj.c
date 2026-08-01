// wynimg_proj - WynCanvas's layered document container (".wync").
//
// ===========================================================================
// WHY A NEW FORMAT AND NOT PSD
// ===========================================================================
//
// PNG export flattens. A document that survives a restart with its layer stack
// intact needs a container that stores layers, and the obvious candidate is
// PSD. It was rejected deliberately:
//
//   * PSD is genuinely large. The layer-and-mask information section alone is
//     a nested tagged structure with per-layer channel image data in four
//     interchangeable compressions (raw / RLE / ZIP / ZIP-with-prediction),
//     global layer-mask info, dozens of additional-layer-info keys, and two
//     incompatible length-field widths (PSD vs PSB). Writing a *reader* that
//     is correct on files produced by real applications is a multi-day job,
//     and the layered part is the least specified part of the public docs.
//   * A half-correct PSD reader is worse than none. Misparsing a channel
//     length silently yields shifted pixel data - an unrecoverable corruption
//     of the user's document that looks like a rendering bug.
//   * WynCanvas's pixels are float32 LINEAR PREMULTIPLIED. PSD has no such
//     layer format, so a PSD round trip would quantise to 8/16-bit sRGB and
//     lose exactly the precision the whole imaging core exists to keep.
//
// So this is WynCanvas's own container: small enough to be obviously correct,
// exact for the pixel format it holds, and versioned so a future reader can
// refuse rather than guess. PSD import remains unimplemented and is documented
// as such rather than half-shipped.
//
// ===========================================================================
// ON-DISK LAYOUT - little-endian, explicit widths, NO STRUCT DUMPING
// ===========================================================================
//
// Every integer is written byte-by-byte in little-endian order and every float
// is written as its IEEE-754 bit pattern in little-endian order. Nothing is
// `fwrite`d from a struct. A struct dump embeds THIS compiler's field padding
// and THIS machine's byte order in the file, which produces a format that
// works perfectly on the machine that wrote it and silently misparses on
// someone else's - the worst possible failure mode for a document format,
// because it appears only after a user has shared a file.
//
//   header (20 bytes)
//     +0   4   magic  'W','Y','N','C'
//     +4   4   int32   version              (WYNPROJ_VERSION)
//     +8   4   int32   document width
//     +12  4   int32   document height
//     +16  4   int32   layer count
//
//   then `layer count` records, bottom layer first (stack order == file order):
//     +0   4   int32   name length in bytes (no NUL stored)
//          n   name bytes
//          4   int32   kind        (0 raster, 1 brightness adjustment)
//          4   int32   blend mode  (0..26, the wynimg_blend_px numbering)
//          4   float32 opacity
//          4   int32   visible     (0/1)
//          4   float32 adjustment amount
//          4   int32   has pixels  (0/1 - adjustment layers have none)
//          4   int32   has mask    (0/1)
//       if has pixels:
//          4   int32   pixel buffer width
//          4   int32   pixel buffer height
//          8   int64   uncompressed byte count
//          8   int64   compressed byte count
//          m           zlib stream (raw float32 RGBA, linear, premultiplied)
//       if has mask:
//          ... the same five fields again for the mask coverage buffer
//
// The uncompressed size is stored so the reader allocates the destination
// EXACTLY ONCE and hands zlib a known destLen, instead of growing a buffer
// while inflating. Float32 RGBA is 16 bytes/pixel, so a 4MP document is 64MB
// raw; zlib is not optional at that size.
//
// COMPRESSION IS LOSSLESS ON THE BYTES, so the float32 round trip is
// bit-identical. tests/test_project.wyn asserts equality, not approximity: if
// a value comes back merely close, something in the path converted through
// double or through sRGB, and that is a bug rather than an accepted tolerance.
//
// ===========================================================================
// EVERY LENGTH IS VALIDATED BEFORE IT IS USED TO ALLOCATE
// ===========================================================================
//
// A corrupt or hostile file is the normal case for a reader, not the
// exceptional one. `int32 layer_count = 0x7FFFFFFF` must be an error return,
// not a two-billion-element calloc; a name length must be bounded before it is
// used; dimensions go through the same WYNIMG_MAX_PIXELS reasoning as
// wynimg_new. The reader knows the total file size up front and refuses any
// declared length that exceeds the bytes actually remaining, so a file
// truncated mid-pixel-data fails at the point of truncation rather than
// reading past the buffer.
//
// Every failure mode returns its OWN non-zero code (WYNPROJ_E_*) because
// "loading failed" is not actionable: the UI needs to distinguish "this is a
// newer file than I understand" from "this file is damaged".
//
// ===========================================================================
// HANDLE DISCIPLINE
// ===========================================================================
//
// Handles arrive from Wyn as `long long` and every one of them is resolved
// through wynimg_deref(), exactly as the rest of the shim does. A stale handle
// resolves to NULL and is treated as "this layer has no pixels" - a safe no-op
// rather than a use-after-free inside the compressor.
//
// On the load side the result table OWNS the buffers it allocated.
// wynproj_take_buffer / wynproj_take_mask transfer one handle to the caller
// (the integrator wiring rows into src/layer.wyn); wynproj_release frees every
// handle NOT taken. A load that fails part way frees everything it had already
// allocated before returning, so a corrupt file cannot leak a document's worth
// of pixels.
//
// FFI CONTRACT, as everywhere in this shim: Wyn `int` <-> C `long long`,
// Wyn `float` <-> C `double`. A C function declared `float` bound to a Wyn
// `-> float` returns garbage silently.

#include "wynimg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

// ---------------------------------------------------------------------------
// Format constants and limits
// ---------------------------------------------------------------------------

// v2 added a per-layer PARENT index (layer groups). A v1 file is still read:
// see the version gate in wynproj_load, which accepts any version it knows how to
// parse rather than only the current one. Refusing v1 would have made every
// existing .wync unloadable for one new int32.
#define WYNPROJ_VERSION 2

// A layer name is a UI label, not a payload. 4096 bytes is absurdly generous
// for one and small enough that a corrupt length is rejected instead of
// becoming a multi-gigabyte allocation.
#define WYNPROJ_MAX_NAME 4096

// Documents here have tens of layers. The cap exists so that a corrupt count
// is refused; it is not a design ceiling anyone will meet.
#define WYNPROJ_MAX_LAYERS 4096

// The largest file this reader will consider. Bounded because the reader
// validates declared lengths against "bytes remaining in the file", which is
// only meaningful if the size itself is sane. 2 GiB of compressed float32 is
// far past any real document.
#define WYNPROJ_MAX_FILE 2147483648LL

// Error codes. Each failure mode is distinguishable on purpose: a silent
// misparse of pixel data is unrecoverable, so the caller must be able to tell
// "newer version" from "damaged bytes" from "I cannot open that path".
#define WYNPROJ_OK              0
#define WYNPROJ_E_OPEN_READ     1
#define WYNPROJ_E_MAGIC         2
#define WYNPROJ_E_VERSION       3
#define WYNPROJ_E_TRUNCATED     4
#define WYNPROJ_E_DIMENSIONS    5
#define WYNPROJ_E_LAYER_COUNT   6
#define WYNPROJ_E_NAME_LEN      7
#define WYNPROJ_E_MEMORY        8
#define WYNPROJ_E_ZLIB          9
#define WYNPROJ_E_OPEN_WRITE   10
#define WYNPROJ_E_WRITE        11
#define WYNPROJ_E_COMPRESS     12
#define WYNPROJ_E_NO_DOC       13
#define WYNPROJ_E_PAYLOAD_SIZE 14
#define WYNPROJ_E_FILE_SIZE    15

// The file stores 4 bytes per float. If a target ever disagreed, the pixel
// data would be written at the wrong stride and read back as garbage, so this
// is a build-time failure rather than a runtime surprise.
_Static_assert(sizeof(float) == 4, "wync stores float32; sizeof(float) must be 4");

// ---------------------------------------------------------------------------
// Little-endian primitives
// ---------------------------------------------------------------------------

static void put_le32(unsigned char* d, unsigned int v) {
    d[0] = (unsigned char)(v & 0xFF);
    d[1] = (unsigned char)((v >> 8) & 0xFF);
    d[2] = (unsigned char)((v >> 16) & 0xFF);
    d[3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned int get_le32(const unsigned char* d) {
    return (unsigned int)d[0]
         | ((unsigned int)d[1] << 8)
         | ((unsigned int)d[2] << 16)
         | ((unsigned int)d[3] << 24);
}

static void put_le64(unsigned char* d, unsigned long long v) {
    for (int i = 0; i < 8; i++) d[i] = (unsigned char)((v >> (8 * i)) & 0xFF);
}

static unsigned long long get_le64(const unsigned char* d) {
    unsigned long long v = 0;
    for (int i = 0; i < 8; i++) v |= (unsigned long long)d[i] << (8 * i);
    return v;
}

// float32 is transported as its bit pattern. memcpy rather than a pointer cast
// so this stays free of strict-aliasing undefined behaviour.
static unsigned int f32_bits(float f) {
    unsigned int u;
    memcpy(&u, &f, 4);
    return u;
}

static float bits_f32(unsigned int u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

// ---------------------------------------------------------------------------
// SAVE SIDE
//
// Wyn cannot hand C an array of layer records (a Wyn array cell is a 16-byte
// tagged union and a struct does not cross a module boundary reliably), so the
// stack is staged one row at a time: save_begin, save_layer * n, save_finish.
//
// Pixels are COMPRESSED AT STAGING TIME rather than at finish. Two reasons,
// both real: peak memory is one compressed blob per layer instead of the whole
// document twice, and the snapshot is taken while the caller still holds the
// handle, so a buffer freed between staging and finish cannot turn a staged
// layer into an empty one.
// ---------------------------------------------------------------------------

typedef struct {
    unsigned char* data;   // zlib stream, or NULL if the layer has none
    long long      raw;    // uncompressed byte count
    long long      comp;   // compressed byte count
    long long      w, h;
} ProjBlob;

typedef struct {
    char*     name;        // NUL-terminated copy, owned
    long long name_len;
    long long kind;
    long long blend;
    float     opacity;
    long long visible;
    float     amount;
    long long parent;          // group index, or -1 for the top level (v2+)
    ProjBlob  pix;
    ProjBlob  mask;
    int       has_pix;
    int       has_mask;
} ProjLayer;

static ProjLayer* g_stage = NULL;
static long long  g_stage_len = 0;
static long long  g_stage_cap = 0;
static long long  g_stage_w = 0;
static long long  g_stage_h = 0;
static int        g_stage_open = 0;

static void blob_clear(ProjBlob* b) {
    free(b->data);
    b->data = NULL;
    b->raw = 0;
    b->comp = 0;
    b->w = 0;
    b->h = 0;
}

static void stage_reset(void) {
    for (long long i = 0; i < g_stage_len; i++) {
        free(g_stage[i].name);
        blob_clear(&g_stage[i].pix);
        blob_clear(&g_stage[i].mask);
    }
    g_stage_len = 0;
    g_stage_w = 0;
    g_stage_h = 0;
    g_stage_open = 0;
}

// Compresses a buffer's raw float32 bytes into `out`. Returns a WYNPROJ_E_*
// code. A NULL/stale handle is not an error - it means "no pixels", which is
// exactly what an adjustment layer has.
static long long blob_from_handle(void* handle, ProjBlob* out, int* present) {
    *present = 0;
    blob_clear(out);
    WynImg* im = wynimg_deref(handle);
    if (!im || !im->px) return WYNPROJ_OK;

    long long raw = im->w * im->h * 4 * 4;   // 4 channels * 4 bytes
    uLongf bound = compressBound((uLong)raw);
    unsigned char* buf = (unsigned char*)malloc(bound);
    if (!buf) return WYNPROJ_E_MEMORY;

    uLongf got = bound;
    int rc = compress2(buf, &got, (const Bytef*)im->px, (uLong)raw,
                       Z_DEFAULT_COMPRESSION);
    if (rc != Z_OK) {
        free(buf);
        return WYNPROJ_E_COMPRESS;
    }
    out->data = buf;
    out->raw = raw;
    out->comp = (long long)got;
    out->w = im->w;
    out->h = im->h;
    *present = 1;
    return WYNPROJ_OK;
}

// Starts a new document. Any half-staged previous document is discarded, so an
// aborted save cannot bleed layers into the next one.
long long wynproj_save_begin(long long w, long long h) {
    stage_reset();
    if (w <= 0 || h <= 0) return WYNPROJ_E_DIMENSIONS;
    if (w > WYNIMG_MAX_PIXELS || h > WYNIMG_MAX_PIXELS) return WYNPROJ_E_DIMENSIONS;
    if (w * h > WYNIMG_MAX_PIXELS) return WYNPROJ_E_DIMENSIONS;
    g_stage_w = w;
    g_stage_h = h;
    g_stage_open = 1;
    return WYNPROJ_OK;
}

long long wynproj_save_layer(const char* name, long long kind, long long blend,
                             double opacity, long long visible, double amount,
                             void* buf, void* mask, long long parent) {
    if (!g_stage_open) return WYNPROJ_E_NO_DOC;
    if (g_stage_len >= WYNPROJ_MAX_LAYERS) return WYNPROJ_E_LAYER_COUNT;

    if (g_stage_len == g_stage_cap) {
        long long ncap = g_stage_cap ? g_stage_cap * 2 : 16;
        ProjLayer* ns = (ProjLayer*)realloc(g_stage, (size_t)ncap * sizeof(ProjLayer));
        if (!ns) return WYNPROJ_E_MEMORY;
        g_stage = ns;
        g_stage_cap = ncap;
    }

    ProjLayer* L = &g_stage[g_stage_len];
    memset(L, 0, sizeof(*L));

    const char* nm = name ? name : "";
    size_t nlen = strlen(nm);
    if (nlen > WYNPROJ_MAX_NAME) return WYNPROJ_E_NAME_LEN;
    L->name = (char*)malloc(nlen + 1);
    if (!L->name) return WYNPROJ_E_MEMORY;
    memcpy(L->name, nm, nlen + 1);
    L->name_len = (long long)nlen;

    L->kind = kind;
    // Clamp rather than reject: a blend index the compositor would ignore must
    // not be able to write a file that a stricter future reader refuses.
    L->blend = (blend < 0 || blend >= WYNIMG_BLEND_COUNT) ? 0 : blend;
    // Stored as float32 because that is the precision the pixels have; a
    // double would imply the compositor honours more than it does.
    L->opacity = (float)(opacity < 0.0 ? 0.0 : (opacity > 1.0 ? 1.0 : opacity));
    L->visible = visible ? 1 : 0;
    L->amount = (float)amount;
    L->parent = parent;

    long long rc = blob_from_handle(buf, &L->pix, &L->has_pix);
    if (rc != WYNPROJ_OK) { free(L->name); L->name = NULL; return rc; }
    rc = blob_from_handle(mask, &L->mask, &L->has_mask);
    if (rc != WYNPROJ_OK) {
        blob_clear(&L->pix);
        free(L->name);
        L->name = NULL;
        return rc;
    }

    g_stage_len++;
    return WYNPROJ_OK;
}

// A tiny writer that latches the first failure, so the emit code below reads
// as a straight sequence of fields instead of a ladder of if(fwrite...).
typedef struct { FILE* f; int bad; } ProjOut;

static void w_bytes(ProjOut* o, const void* p, size_t n) {
    if (o->bad || n == 0) return;
    if (fwrite(p, 1, n, o->f) != n) o->bad = 1;
}
static void w_u32(ProjOut* o, unsigned int v) {
    unsigned char b[4];
    put_le32(b, v);
    w_bytes(o, b, 4);
}
static void w_i64(ProjOut* o, long long v) {
    unsigned char b[8];
    put_le64(b, (unsigned long long)v);
    w_bytes(o, b, 8);
}
static void w_f32(ProjOut* o, float v) { w_u32(o, f32_bits(v)); }

static void w_blob(ProjOut* o, const ProjBlob* b) {
    w_u32(o, (unsigned int)b->w);
    w_u32(o, (unsigned int)b->h);
    w_i64(o, b->raw);
    w_i64(o, b->comp);
    w_bytes(o, b->data, (size_t)b->comp);
}

// Writes the staged document to `path` and clears the staging area either way.
// Returns 0 on success, else a WYNPROJ_E_* code.
long long wynproj_save_finish(const char* path) {
    if (!g_stage_open) { stage_reset(); return WYNPROJ_E_NO_DOC; }
    if (!path) { stage_reset(); return WYNPROJ_E_OPEN_WRITE; }

    FILE* f = fopen(path, "wb");
    if (!f) { stage_reset(); return WYNPROJ_E_OPEN_WRITE; }

    ProjOut o = { f, 0 };
    w_bytes(&o, "WYNC", 4);
    w_u32(&o, WYNPROJ_VERSION);
    w_u32(&o, (unsigned int)g_stage_w);
    w_u32(&o, (unsigned int)g_stage_h);
    w_u32(&o, (unsigned int)g_stage_len);

    for (long long i = 0; i < g_stage_len; i++) {
        const ProjLayer* L = &g_stage[i];
        w_u32(&o, (unsigned int)L->name_len);
        w_bytes(&o, L->name, (size_t)L->name_len);
        w_u32(&o, (unsigned int)L->kind);
        w_u32(&o, (unsigned int)L->blend);
        w_f32(&o, L->opacity);
        w_u32(&o, (unsigned int)L->visible);
        w_f32(&o, L->amount);
        // v2: the group index. Written as an int32 two's-complement, so -1 (top
        // level) round-trips through the unsigned reader as 0xFFFFFFFF and is cast
        // back on the way in - see the matching read.
        w_u32(&o, (unsigned int)(int)L->parent);
        w_u32(&o, (unsigned int)L->has_pix);
        w_u32(&o, (unsigned int)L->has_mask);
        if (L->has_pix)  w_blob(&o, &L->pix);
        if (L->has_mask) w_blob(&o, &L->mask);
    }

    // fclose can fail where fwrite did not: buffered bytes are flushed here,
    // so a full disk surfaces at close. Reporting success then would tell the
    // user their document is saved when it is not.
    int closed_bad = (fclose(f) != 0);
    stage_reset();
    if (o.bad || closed_bad) return WYNPROJ_E_WRITE;
    return WYNPROJ_OK;
}

// ---------------------------------------------------------------------------
// LOAD SIDE
// ---------------------------------------------------------------------------

typedef struct {
    char*     name;
    long long kind;
    long long blend;
    float     opacity;
    long long visible;
    float     amount;
    long long parent;          // group index, or -1 (v1 files: always -1)
    void*     buf;     // wynimg handle, 0 if the layer has no pixels
    void*     mask;    // wynimg handle, 0 if the layer has no mask
} LoadLayer;

static LoadLayer* g_load = NULL;
static long long  g_load_len = 0;
static long long  g_load_w = 0;
static long long  g_load_h = 0;

// Frees every handle the load table still owns. Handles removed by
// wynproj_take_* are already zeroed and are therefore skipped.
static void load_reset(void) {
    for (long long i = 0; i < g_load_len; i++) {
        free(g_load[i].name);
        wynimg_free(g_load[i].buf);
        wynimg_free(g_load[i].mask);
    }
    free(g_load);
    g_load = NULL;
    g_load_len = 0;
    g_load_w = 0;
    g_load_h = 0;
}

// Cursor over the file. `total` is the real size on disk, so every declared
// length can be checked against the bytes that actually remain BEFORE any
// allocation happens. This is what makes a truncated file fail at the point of
// truncation instead of reading past a buffer.
typedef struct {
    FILE*     f;
    long long pos;
    long long total;
} ProjIn;

static long long rd_bytes(ProjIn* in, void* dst, long long n) {
    if (n < 0) return WYNPROJ_E_TRUNCATED;
    if (n > in->total - in->pos) return WYNPROJ_E_TRUNCATED;
    if (n == 0) return WYNPROJ_OK;
    if (fread(dst, 1, (size_t)n, in->f) != (size_t)n) return WYNPROJ_E_TRUNCATED;
    in->pos += n;
    return WYNPROJ_OK;
}

static long long rd_u32(ProjIn* in, unsigned int* out) {
    unsigned char b[4];
    long long rc = rd_bytes(in, b, 4);
    if (rc != WYNPROJ_OK) return rc;
    *out = get_le32(b);
    return WYNPROJ_OK;
}

static long long rd_i64(ProjIn* in, long long* out) {
    unsigned char b[8];
    long long rc = rd_bytes(in, b, 8);
    if (rc != WYNPROJ_OK) return rc;
    *out = (long long)get_le64(b);
    return WYNPROJ_OK;
}

static long long rd_f32(ProjIn* in, float* out) {
    unsigned int u;
    long long rc = rd_u32(in, &u);
    if (rc != WYNPROJ_OK) return rc;
    *out = bits_f32(u);
    return WYNPROJ_OK;
}

// Reads one pixel/mask blob and inflates it straight into a fresh wynimg
// buffer. `*handle_out` is 0 unless this returns WYNPROJ_OK.
static long long rd_blob(ProjIn* in, void** handle_out) {
    *handle_out = NULL;

    unsigned int uw = 0, uh = 0;
    long long rc = rd_u32(in, &uw);
    if (rc != WYNPROJ_OK) return rc;
    rc = rd_u32(in, &uh);
    if (rc != WYNPROJ_OK) return rc;

    long long w = (long long)uw;
    long long h = (long long)uh;
    // Same reasoning as wynimg_new: refuse the dimension rather than calloc
    // from it. `uw` is a 32-bit field so w and h are each < 2^32 and the
    // product cannot overflow long long.
    if (w <= 0 || h <= 0) return WYNPROJ_E_DIMENSIONS;
    if (w * h > WYNIMG_MAX_PIXELS) return WYNPROJ_E_DIMENSIONS;

    long long raw = 0, comp = 0;
    rc = rd_i64(in, &raw);
    if (rc != WYNPROJ_OK) return rc;
    rc = rd_i64(in, &comp);
    if (rc != WYNPROJ_OK) return rc;

    // The declared uncompressed size must be exactly what these dimensions
    // imply. If it is not, the file disagrees with itself and inflating would
    // write the wrong number of bytes into a correctly-sized buffer - a
    // shifted image, which is the unrecoverable failure this check exists to
    // prevent.
    if (raw != w * h * 4 * 4) return WYNPROJ_E_PAYLOAD_SIZE;
    if (comp < 0 || comp > in->total - in->pos) return WYNPROJ_E_TRUNCATED;

    unsigned char* cbuf = (unsigned char*)malloc((size_t)comp ? (size_t)comp : 1);
    if (!cbuf) return WYNPROJ_E_MEMORY;
    rc = rd_bytes(in, cbuf, comp);
    if (rc != WYNPROJ_OK) { free(cbuf); return rc; }

    void* handle = wynimg_new(w, h);
    if (!handle) { free(cbuf); return WYNPROJ_E_MEMORY; }
    WynImg* im = wynimg_deref(handle);
    if (!im || !im->px) { free(cbuf); wynimg_free(handle); return WYNPROJ_E_MEMORY; }

    // Allocated exactly once, with destLen known from the stored raw size.
    uLongf dlen = (uLongf)raw;
    int zrc = uncompress((Bytef*)im->px, &dlen, (const Bytef*)cbuf, (uLong)comp);
    free(cbuf);
    if (zrc != Z_OK || (long long)dlen != raw) {
        wynimg_free(handle);
        return WYNPROJ_E_ZLIB;
    }

    *handle_out = handle;
    return WYNPROJ_OK;
}

// Loads `path`. Returns 0 on success, else a WYNPROJ_E_* code. On ANY failure
// every buffer already allocated is freed before returning, so a corrupt file
// cannot leak a document's worth of pixels; on success the result table owns
// the handles until wynproj_take_* or wynproj_release.
long long wynproj_load(const char* path) {
    load_reset();
    if (!path) return WYNPROJ_E_OPEN_READ;

    FILE* f = fopen(path, "rb");
    if (!f) return WYNPROJ_E_OPEN_READ;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return WYNPROJ_E_OPEN_READ; }
    long off = ftell(f);
    if (off < 0) { fclose(f); return WYNPROJ_E_OPEN_READ; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return WYNPROJ_E_OPEN_READ; }
    if ((long long)off > WYNPROJ_MAX_FILE) { fclose(f); return WYNPROJ_E_FILE_SIZE; }

    ProjIn in = { f, 0, (long long)off };

    unsigned char magic[4];
    long long rc = rd_bytes(&in, magic, 4);
    if (rc != WYNPROJ_OK) { fclose(f); return rc; }
    if (memcmp(magic, "WYNC", 4) != 0) { fclose(f); return WYNPROJ_E_MAGIC; }

    unsigned int ver = 0;
    rc = rd_u32(&in, &ver);
    if (rc != WYNPROJ_OK) { fclose(f); return rc; }
    // REFUSE, do not guess. Reading a v2 layout with v1 field offsets would
    // misparse pixel lengths and produce a plausible-looking but corrupt
    // document, which the user cannot recover from or even diagnose.
    // REFUSE A FUTURE VERSION, READ A PAST ONE. Reading a newer layout with older
    // field offsets would produce silent garbage, so anything above the current
    // version is refused as before. But refusing v1 would have made every
    // existing .wync unloadable in exchange for one new int32, and the v1 layout
    // is a strict prefix of v2 - so it is parsed, with `parent` defaulting to the
    // top level. `ver` is threaded into the record loop below for exactly this.
    if (ver == 0 || ver > WYNPROJ_VERSION) { fclose(f); return WYNPROJ_E_VERSION; }

    unsigned int dw = 0, dh = 0, lc = 0;
    rc = rd_u32(&in, &dw);
    if (rc != WYNPROJ_OK) { fclose(f); return rc; }
    rc = rd_u32(&in, &dh);
    if (rc != WYNPROJ_OK) { fclose(f); return rc; }
    rc = rd_u32(&in, &lc);
    if (rc != WYNPROJ_OK) { fclose(f); return rc; }

    if (dw == 0 || dh == 0) { fclose(f); return WYNPROJ_E_DIMENSIONS; }
    if ((long long)dw * (long long)dh > WYNIMG_MAX_PIXELS) {
        fclose(f);
        return WYNPROJ_E_DIMENSIONS;
    }
    // Validated BEFORE the calloc below. A corrupt 0x7FFFFFFF here would
    // otherwise be a 2-billion-element allocation request.
    if (lc > WYNPROJ_MAX_LAYERS) { fclose(f); return WYNPROJ_E_LAYER_COUNT; }

    LoadLayer* rows = NULL;
    if (lc > 0) {
        rows = (LoadLayer*)calloc((size_t)lc, sizeof(LoadLayer));
        if (!rows) { fclose(f); return WYNPROJ_E_MEMORY; }
    }
    // Published immediately so the single load_reset() below frees whatever was
    // built so far, no matter which field fails.
    g_load = rows;
    g_load_len = (long long)lc;
    g_load_w = (long long)dw;
    g_load_h = (long long)dh;

    for (unsigned int i = 0; i < lc; i++) {
        LoadLayer* L = &g_load[i];

        unsigned int nlen = 0;
        rc = rd_u32(&in, &nlen);
        if (rc != WYNPROJ_OK) goto fail;
        // Bounded before the malloc, and against the remaining file besides:
        // a plausible-looking 3000-byte name in a 40-byte file is corruption.
        if (nlen > WYNPROJ_MAX_NAME) { rc = WYNPROJ_E_NAME_LEN; goto fail; }
        if ((long long)nlen > in.total - in.pos) { rc = WYNPROJ_E_TRUNCATED; goto fail; }
        L->name = (char*)malloc((size_t)nlen + 1);
        if (!L->name) { rc = WYNPROJ_E_MEMORY; goto fail; }
        rc = rd_bytes(&in, L->name, (long long)nlen);
        if (rc != WYNPROJ_OK) goto fail;
        L->name[nlen] = '\0';

        unsigned int kind = 0, blend = 0, vis = 0, hasp = 0, hasm = 0;
        rc = rd_u32(&in, &kind);   if (rc != WYNPROJ_OK) goto fail;
        rc = rd_u32(&in, &blend);  if (rc != WYNPROJ_OK) goto fail;
        rc = rd_f32(&in, &L->opacity); if (rc != WYNPROJ_OK) goto fail;
        rc = rd_u32(&in, &vis);    if (rc != WYNPROJ_OK) goto fail;
        rc = rd_f32(&in, &L->amount);  if (rc != WYNPROJ_OK) goto fail;
        // v2 only. A v1 file has no parent field at all, and every layer in one
        // is top-level by definition - so defaulting to -1 is not a guess, it is
        // what a flat document means.
        L->parent = -1;
        if (ver >= 2) {
            unsigned int par = 0;
            rc = rd_u32(&in, &par);  if (rc != WYNPROJ_OK) goto fail;
            L->parent = (long long)(int)par;
        }
        rc = rd_u32(&in, &hasp);   if (rc != WYNPROJ_OK) goto fail;
        rc = rd_u32(&in, &hasm);   if (rc != WYNPROJ_OK) goto fail;

        L->kind = (long long)kind;
        // A blend index outside the compositor's table would index past the
        // kernel array, so it is coerced here rather than trusted.
        L->blend = (blend >= WYNIMG_BLEND_COUNT) ? 0 : (long long)blend;
        L->visible = vis ? 1 : 0;

        if (hasp) {
            rc = rd_blob(&in, &L->buf);
            if (rc != WYNPROJ_OK) goto fail;
        }
        if (hasm) {
            rc = rd_blob(&in, &L->mask);
            if (rc != WYNPROJ_OK) goto fail;
        }
    }

    fclose(f);
    return WYNPROJ_OK;

fail:
    fclose(f);
    load_reset();
    return rc;
}

static int load_valid(long long i) { return i >= 0 && i < g_load_len; }

long long wynproj_doc_width(void)   { return g_load_w; }
long long wynproj_doc_height(void)  { return g_load_h; }
long long wynproj_layer_count(void) { return g_load_len; }

// Returns a pointer into the load table, valid until the next load or release.
// Wyn copies it into a Wyn string at the call site.
const char* wynproj_layer_name(long long i) {
    if (!load_valid(i) || !g_load[i].name) return "";
    return g_load[i].name;
}

long long wynproj_layer_kind(long long i)    { return load_valid(i) ? g_load[i].kind : 0; }
long long wynproj_layer_blend(long long i)   { return load_valid(i) ? g_load[i].blend : 0; }
double    wynproj_layer_opacity(long long i) { return load_valid(i) ? (double)g_load[i].opacity : 1.0; }
long long wynproj_layer_visible(long long i) { return load_valid(i) ? g_load[i].visible : 0; }
double    wynproj_layer_amount(long long i)  { return load_valid(i) ? (double)g_load[i].amount : 0.0; }
// -1 for a top-level layer, and for EVERY layer in a v1 file.
long long wynproj_layer_parent(long long i)  { return load_valid(i) ? g_load[i].parent : -1; }

// Borrowing accessors: the table keeps ownership, so these are safe to read
// repeatedly (tests do) without risking a double free.
void* wynproj_layer_buffer(long long i) { return load_valid(i) ? g_load[i].buf : NULL; }
void* wynproj_layer_mask(long long i)   { return load_valid(i) ? g_load[i].mask : NULL; }

// Transferring accessors: the handle leaves the table, so wynproj_release will
// not free it. The integrator uses these when a row goes into src/layer.wyn,
// which then owns the buffer for the rest of the session. Calling take twice
// yields 0 the second time rather than the same handle twice - handing the same
// buffer to two owners is how a double free happens.
void* wynproj_take_buffer(long long i) {
    if (!load_valid(i)) return NULL;
    void* h = g_load[i].buf;
    g_load[i].buf = NULL;
    return h;
}

void* wynproj_take_mask(long long i) {
    if (!load_valid(i)) return NULL;
    void* h = g_load[i].mask;
    g_load[i].mask = NULL;
    return h;
}

// Frees every handle not taken, and the table. Idempotent.
void wynproj_release(void) { load_reset(); }

const char* wynproj_error_message(long long code) {
    switch (code) {
        case WYNPROJ_OK:              return "ok";
        case WYNPROJ_E_OPEN_READ:     return "cannot open project file for reading";
        case WYNPROJ_E_MAGIC:         return "not a WynCanvas project (bad magic)";
        case WYNPROJ_E_VERSION:       return "unsupported project version";
        case WYNPROJ_E_TRUNCATED:     return "project file is truncated";
        case WYNPROJ_E_DIMENSIONS:    return "invalid dimensions in project file";
        case WYNPROJ_E_LAYER_COUNT:   return "invalid layer count in project file";
        case WYNPROJ_E_NAME_LEN:      return "invalid layer name length in project file";
        case WYNPROJ_E_MEMORY:        return "out of memory";
        case WYNPROJ_E_ZLIB:          return "corrupt compressed pixel data";
        case WYNPROJ_E_OPEN_WRITE:    return "cannot open project file for writing";
        case WYNPROJ_E_WRITE:         return "write failed while saving project";
        case WYNPROJ_E_COMPRESS:      return "compression failed while saving project";
        case WYNPROJ_E_NO_DOC:        return "no document staged for saving";
        case WYNPROJ_E_PAYLOAD_SIZE:  return "pixel payload size disagrees with dimensions";
        case WYNPROJ_E_FILE_SIZE:     return "project file is implausibly large";
        default:                      return "unknown project error";
    }
}

// Exposed so Wyn (and tests) can name codes without duplicating the numbers.
long long wynproj_err_open_read(void)    { return WYNPROJ_E_OPEN_READ; }
long long wynproj_err_magic(void)        { return WYNPROJ_E_MAGIC; }
long long wynproj_err_version(void)      { return WYNPROJ_E_VERSION; }
long long wynproj_err_truncated(void)    { return WYNPROJ_E_TRUNCATED; }
long long wynproj_err_dimensions(void)   { return WYNPROJ_E_DIMENSIONS; }
long long wynproj_err_layer_count(void)  { return WYNPROJ_E_LAYER_COUNT; }
long long wynproj_err_name_len(void)     { return WYNPROJ_E_NAME_LEN; }
long long wynproj_err_zlib(void)         { return WYNPROJ_E_ZLIB; }
long long wynproj_err_payload_size(void) { return WYNPROJ_E_PAYLOAD_SIZE; }
long long wynproj_err_write(void)        { return WYNPROJ_E_WRITE; }
long long wynproj_err_open_write(void)   { return WYNPROJ_E_OPEN_WRITE; }
long long wynproj_version(void)          { return WYNPROJ_VERSION; }

// ---------------------------------------------------------------------------
// TEST SEAMS
//
// The corrupt-file cases are the ones that matter most in a format reader, and
// they can only be exercised by handing the reader damaged bytes. Wyn cannot
// produce those: File.write goes through a NUL-terminated C string, so it can
// neither write an arbitrary byte nor patch one in place. These three do it
// from C, which keeps the fixtures GENERATED rather than committed - a binary
// fixture in the repo is a file nobody can review and that silently rots when
// the format changes.
// ---------------------------------------------------------------------------

// Overwrites one byte of an existing file. Returns 1 on success.
long long wynproj_poke_byte(const char* path, long long off, long long val) {
    if (!path || off < 0) return 0;
    FILE* f = fopen(path, "r+b");
    if (!f) return 0;
    if (fseek(f, (long)off, SEEK_SET) != 0) { fclose(f); return 0; }
    unsigned char b = (unsigned char)(val & 0xFF);
    size_t n = fwrite(&b, 1, 1, f);
    int bad = (fclose(f) != 0);
    return (n == 1 && !bad) ? 1 : 0;
}

// Copies the first `len` bytes of `src` to `dst`. Returns the bytes written, or
// -1 on failure. Used to build a file truncated mid-pixel-data.
long long wynproj_truncate_copy(const char* src, const char* dst, long long len) {
    if (!src || !dst || len < 0) return -1;
    FILE* in = fopen(src, "rb");
    if (!in) return -1;
    FILE* out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    long long written = 0;
    unsigned char buf[4096];
    while (written < len) {
        long long want = len - written;
        if (want > (long long)sizeof buf) want = (long long)sizeof buf;
        size_t got = fread(buf, 1, (size_t)want, in);
        if (got == 0) break;
        if (fwrite(buf, 1, got, out) != got) { written = -1; break; }
        written += (long long)got;
    }
    fclose(in);
    if (fclose(out) != 0) return -1;
    return written;
}

// Byte count of `path`, or -1. Lets a test compute a truncation point.
long long wynproj_file_len(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long n = ftell(f);
    fclose(f);
    return (n < 0) ? -1 : (long long)n;
}

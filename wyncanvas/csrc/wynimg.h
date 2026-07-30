// wynimg - flat float32 RGBA pixel buffers for WynCanvas.
//
// FFI CONTRACT (see plan Global Constraints):
//   Wyn `int`   <-> C `long long`
//   Wyn `float` <-> C `double`
// Every function crossing the FFI boundary MUST use those C types.
// Internally pixels are `float` (half the memory of double).
//
// Pixel format: interleaved RGBA, 4 floats per pixel, LINEAR light,
// PREMULTIPLIED alpha. sRGB encode/decode happens only at I/O edges.
#ifndef WYNIMG_H
#define WYNIMG_H

// Agreed tile size. NOT yet used: cycle 1 allocates whole-image buffers.
// Declared here so the value lives in one place when cycle 2 adds tiling.
#define WYNIMG_TILE 256

// Largest image this shim will allocate, in pixels. 512 megapixels is ~8 GB of
// float32 RGBA -- far beyond any real document, but small enough that
// `w * h * 4 * sizeof(float)` cannot overflow size_t and that a bogus dimension
// is refused instead of being lazily calloc'd and then turned into a
// multi-second page-faulting DoS by the first wynimg_fill.
#define WYNIMG_MAX_PIXELS 536870912LL

typedef struct {
    long long w, h;
    float* px;      // w * h * 4 floats, or NULL on allocation failure
} WynImg;

// HANDLES, NOT POINTERS.
//
// wynimg_new returns an opaque generation-tagged handle, not a WynImg*. Wyn
// cannot be prevented from holding a stale handle -- `wynimg_free(p)` cannot
// null the caller's copy, and src/layer.wyn stores buffer/mask handles that
// outlive their buffers -- so a raw pointer would make every such case a
// use-after-free inside the pixel loops. Freeing a handle bumps its slot
// generation, which makes every later use of the old handle resolve to NULL and
// take the same safe path as a null argument.
//
// Returns 0 on allocation failure, non-positive dimensions, or a pixel count
// above WYNIMG_MAX_PIXELS.
void* wynimg_new(long long w, long long h);

// Releases the buffer and invalidates the handle. Safe on 0 and on an already
// freed or otherwise stale handle (both are no-ops, not double frees).
void  wynimg_free(void* p);

// 1 if the handle currently refers to a live buffer, else 0. Exposed so tests
// can assert that freeing actually invalidates.
long long wynimg_alive(void* p);

// Resolves a handle to its buffer, or NULL if it is null/stale/freed. Internal
// to the shim -- every function that takes a handle from Wyn MUST go through
// this rather than casting, or it reintroduces the use-after-free.
WynImg* wynimg_deref(void* p);

long long wynimg_width(void* p);
long long wynimg_height(void* p);

// Bounds-checked. get returns 0.0 out of range; set is a no-op out of range.
double wynimg_get(void* p, long long idx);
void   wynimg_set(void* p, long long idx, double v);

void wynimg_fill(void* p, double r, double g, double b, double a);

// Separable blend modes, indexed 0..26 in this fixed order. The numbering is
// part of the FFI contract and must match src/pixel.wyn.
//  0 normal        1 multiply      2 screen        3 overlay
//  4 darken        5 lighten       6 color_dodge   7 color_burn
//  8 hard_light    9 soft_light   10 linear_dodge 11 linear_burn
// 12 linear_light 13 vivid_light  14 difference   15 exclusion
// 16 subtract     17 divide       18 pin_light    19 hard_mix
// 20 darker_color 21 lighter_color
// 22 hue          23 saturation   24 color        25 luminosity
// 26 dissolve
#define WYNIMG_BLEND_COUNT 27

// Blends one UNPREMULTIPLIED channel pair. cb = backdrop, cs = source.
// Result is clamped to [0,1]. Non-separable modes (22-25) and dissolve (26)
// fall back to `normal` here; they need full-pixel context and are handled
// in the compositor.
double wynimg_blend_px(long long mode, double cb, double cs);

// PNG I/O. Files are 8-bit sRGB; buffers are float32 linear, so these
// functions apply the sRGB transfer function on the way in and out.
// All libpng error handling (longjmp) is contained here and never crosses
// the FFI boundary.
void*     wynimg_load_png(const char* path);   // NULL on any failure
long long wynimg_save_png(void* p, const char* path); // 1 ok, 0 fail

// Loads a PNG as a MASK: the R channel becomes linear coverage directly, with
// NO sRGB decode and no premultiply, and alpha is forced to 1.
//
// Coverage is not a colour, so it must not go through the transfer function.
// wynimg_composite reads mask->px[o+0] as linear coverage, so a mask loaded
// with wynimg_load_png would arrive pre-decoded: a 50% grey mask (byte 128)
// became 0.2159 coverage instead of 0.5020 -- a visibly wrong 22% where 50%
// was painted. Use this for masks and wynimg_load_png for imagery.
void*     wynimg_load_mask_png(const char* path);

// Blends `src` over `dst` in place, honouring an optional mask (its R channel
// is the coverage) and a layer opacity. Buffers must share dimensions.
// Returns 1 on success, 0 on mismatch or null input.
long long wynimg_composite(void* dst, void* src, void* mask,
                           long long mode, double opacity);

// Adds `amount` to every colour channel in linear light. Returns 1/0.
long long wynimg_brightness(void* buf, double amount);

// Largest absolute per-channel difference between two buffers.
// Returns 2.0 (impossible for valid [0,1] data) if they cannot be compared,
// so a dimension mismatch can never masquerade as a pass.
double wynimg_max_diff(void* a, void* b);

// Writes a complete HTTP 200 response carrying the bytes of `path` to an
// already-accepted socket. Returns 1 on success, 0 if the file cannot be read
// or the socket write fails.
//
// WHY THIS LIVES IN C. The Wyn runtime cannot serve binary. `File.read` stops
// at the first NUL byte (src/wyn_runtime.h:2735 opens with mode "r" and
// NUL-terminates), and `Http_respond` sizes its body with `strlen`
// (src/wyn_runtime.h:3095). Measured: a 21-byte file with one interior NUL was
// served as 12 bytes. Every PNG has NULs in its IHDR, so any Wyn-side
// read-and-send corrupts the image.
//
// This is also the architecturally correct home for it: the plan's rule is that
// bulk pixels never pass through Wyn. C already writes the PNG; C now also
// hands it to the socket. Wyn passes a path and an fd - two scalars.
//
// The response advertises `Connection: close` and shuts the write side down, so
// the caller must close the fd afterwards (Http.close_client).
long long wynimg_http_send_file(long long fd, const char* path,
                                const char* content_type);

// Test seam for the above. Wyn's File.open returns an index into a FILE* table
// (wyn_runtime.h:2982), not a descriptor, so a test cannot otherwise hand
// wynimg_http_send_file anything to write to. These give it a real fd and a
// byte-exact way to inspect the result -- necessary because the bug being fixed
// is invisible to any check that goes through a Wyn string.
long long wynimg_open_wr(const char* path);      // fd, or -1
long long wynimg_close_fd(long long fd);         // 1 ok, 0 fail
long long wynimg_file_size(const char* path);    // bytes, or -1
long long wynimg_file_byte(const char* path, long long off); // 0..255, or -1

#endif

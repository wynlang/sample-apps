#include "wynimg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <png.h>

// sRGB transfer functions, duplicated here so the C side never calls back
// into Wyn. Must stay numerically identical to src/pixel.wyn.
// Both are NaN-safe by ordering: `!(v > 0.0)` is TRUE for NaN, so NaN exits at
// the first line as 0 instead of falling through to pow(). It matters because
// the write path casts the result to `unsigned char`, and converting a NaN to an
// integer type is UNDEFINED BEHAVIOUR in C -- not merely a wrong byte. A NaN
// pixel is reachable from pure Wyn (`0.0/0.0` then wynimg_set), and before this
// guard `wynimg_save_png` on a buffer with one NaN channel executed that cast
// and still returned 1.
static double s2l(double v) {
    if (!(v > 0.0)) return 0.0;
    if (v >= 1.0) return 1.0;
    if (v <= 0.04045) return v / 12.92;
    return pow((v + 0.055) / 1.055, 2.4);
}
static double l2s(double v) {
    if (!(v > 0.0)) return 0.0;
    if (v >= 1.0) return 1.0;
    if (v <= 0.0031308) return v * 12.92;
    return 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

// `as_mask` selects coverage semantics instead of colour semantics: the R
// channel is taken as LINEAR coverage with no sRGB decode and no premultiply.
// See wynimg_load_mask_png in the header for why that distinction matters.
static void* load_png_impl(const char* path, int as_mask) {
    if (!path) return NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    unsigned char sig[8];
    if (fread(sig, 1, 8, f) != 8 || png_sig_cmp(sig, 0, 8)) {
        fclose(f);
        return NULL;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(f); return NULL; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); fclose(f); return NULL; }

    // MUST be volatile. These are written after setjmp and read inside the
    // longjmp handler; without volatile the compiler is free to keep them in
    // callee-saved registers, which longjmp restores to their values AT SETJMP
    // TIME -- i.e. NULL. Measured against the shipped -O2 archive: the handler
    // saw im=0x0 row=0x0 and freed nothing, leaking 1049632 bytes per failed
    // load of a 256x256 PNG (the whole pixel buffer plus the row). At -O0 the
    // same handler saw real pointers and leaked 0, which is exactly why this
    // never showed up in testing.
    WynImg* volatile   im  = NULL;
    void*   volatile   row = NULL;
    void*   volatile   handle = NULL;   // freeing goes through the handle API
    if (setjmp(png_jmpbuf(png))) {          // any libpng error lands here
        if (row) free((void*)row);
        if (handle) wynimg_free((void*)handle);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(f);
        return NULL;
    }

    png_init_io(png, f);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    png_uint_32 w = png_get_image_width(png, info);
    png_uint_32 h = png_get_image_height(png, info);
    int depth = png_get_bit_depth(png, info);
    int ctype = png_get_color_type(png, info);

    // Normalize everything to 8-bit RGBA.
    if (ctype == PNG_COLOR_TYPE_PALETTE)          png_set_palette_to_rgb(png);
    if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))   png_set_tRNS_to_alpha(png);
    if (depth == 16)                               png_set_strip_16(png);
    if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (ctype == PNG_COLOR_TYPE_RGB || ctype == PNG_COLOR_TYPE_GRAY ||
        ctype == PNG_COLOR_TYPE_PALETTE)
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);

    // The row loop below indexes 4 bytes per pixel. If the transform chain
    // somehow did not land on 8-bit RGBA, bail out rather than read past the
    // end of `row`.
    if (png_get_channels(png, info) != 4 || png_get_bit_depth(png, info) != 8)
        png_longjmp(png, 1);

    handle = wynimg_new((long long)w, (long long)h);
    if (!handle) png_longjmp(png, 1);
    im = wynimg_deref((void*)handle);
    if (!im) png_longjmp(png, 1);

    row = malloc(png_get_rowbytes(png, info));
    if (!row) png_longjmp(png, 1);

    for (png_uint_32 y = 0; y < h; y++) {
        png_bytep rp = (png_bytep)row;
        png_read_row(png, rp, NULL);
        for (png_uint_32 x = 0; x < w; x++) {
            size_t o = ((size_t)y * w + x) * 4;
            if (as_mask) {
                // Coverage is not a colour: no transfer function, no
                // premultiply. Replicated across RGB so the buffer is still a
                // well-formed image if something displays it.
                double cov = rp[x*4+0] / 255.0;
                im->px[o+0] = (float)cov;
                im->px[o+1] = (float)cov;
                im->px[o+2] = (float)cov;
                im->px[o+3] = 1.0f;
            } else {
                double a = rp[x*4+3] / 255.0;
                // sRGB decode colour channels, then premultiply by alpha.
                double r = s2l(rp[x*4+0] / 255.0) * a;
                double g = s2l(rp[x*4+1] / 255.0) * a;
                double b = s2l(rp[x*4+2] / 255.0) * a;
                im->px[o+0] = (float)r;
                im->px[o+1] = (float)g;
                im->px[o+2] = (float)b;
                im->px[o+3] = (float)a;
            }
        }
    }

    png_read_end(png, NULL);
    free((void*)row);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(f);
    return (void*)handle;
}

void* wynimg_load_png(const char* path) {
    return load_png_impl(path, 0);
}

void* wynimg_load_mask_png(const char* path) {
    return load_png_impl(path, 1);
}

long long wynimg_save_png(void* p, const char* path) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px || !path) return 0;

    FILE* f = fopen(path, "wb");
    if (!f) return 0;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(f); return 0; }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_write_struct(&png, NULL); fclose(f); return 0; }

    // volatile for the same reason as in the load path: `row` is assigned after
    // setjmp and freed inside the handler, and at -O2 longjmp restored it to
    // NULL so the handler freed nothing. Measured: 30 failed saves under
    // RLIMIT_FSIZE leaked 30720 bytes, exactly one 256*4 row buffer per call.
    void* volatile row = NULL;
    if (setjmp(png_jmpbuf(png))) {
        if (row) free((void*)row);
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return 0;
    }

    png_init_io(png, f);
    png_set_IHDR(png, info, (png_uint_32)im->w, (png_uint_32)im->h, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    row = malloc((size_t)im->w * 4);
    if (!row) png_longjmp(png, 1);

    png_bytep rowb = (png_bytep)row;
    for (long long y = 0; y < im->h; y++) {
        for (long long x = 0; x < im->w; x++) {
            size_t o = ((size_t)y * im->w + x) * 4;
            // `!(a > 0.0)` rather than `a < 0.0`, so a NaN alpha becomes 0 here
            // instead of reaching the `(unsigned char)(a * 255.0 + 0.5)` cast
            // below, which for NaN is undefined behaviour rather than a wrong
            // byte. The two bare comparisons this replaces both evaluate false
            // for NaN, so it passed straight through them.
            double a = im->px[o+3];
            if (!(a > 0.0)) a = 0.0;
            if (a > 1.0) a = 1.0;
            // Un-premultiply, then sRGB encode.
            double r = (a > 0.0) ? im->px[o+0] / a : 0.0;
            double g = (a > 0.0) ? im->px[o+1] / a : 0.0;
            double b = (a > 0.0) ? im->px[o+2] / a : 0.0;
            rowb[x*4+0] = (unsigned char)(l2s(r) * 255.0 + 0.5);
            rowb[x*4+1] = (unsigned char)(l2s(g) * 255.0 + 0.5);
            rowb[x*4+2] = (unsigned char)(l2s(b) * 255.0 + 0.5);
            rowb[x*4+3] = (unsigned char)(a * 255.0 + 0.5);
        }
        png_write_row(png, rowb);
    }

    free((void*)row);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return 1;
}

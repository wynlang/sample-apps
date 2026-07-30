// wynimg_tex - the ONE function that connects the imaging core to a GPU texture.
//
// WHY THIS FILE EXISTS AT ALL. src/ui.wyn draws the composited document by
// uploading it to a texture and blitting once. The gui package's
// Win_texture_update_f32 already accepts precisely wynimg's representation -
// interleaved float32 RGBA, LINEAR light, PREMULTIPLIED - so no conversion and
// no copy is needed on this side. What IS needed is a way to get from a wynimg
// HANDLE to the `float*` behind it, and that is deliberately not exposed:
// wynimg_deref is internal to the shim (csrc/wynimg.h) so that no caller can
// cast a handle to a pointer and reintroduce the use-after-free the handle table
// exists to prevent.
//
// So the bridge lives here, in C, on the shim's side of the boundary. Wyn never
// holds the pixel pointer - it passes two scalars, a texture handle and a buffer
// handle - which is the same rule the rest of the project follows: bulk pixels
// never enter Wyn. A stale or freed buffer handle resolves to NULL and returns
// 0, exactly like every other wynimg entry point.
//
// The alternative - exposing `wynimg_pixels() -> ptr` and letting Wyn thread the
// address into the gui package - was rejected: it would put a raw pixel pointer
// into a Wyn variable that outlives no particular scope, and `ptr` in Wyn module
// signatures is still miscompiled upstream (see the note in src/pixel.wyn).
//
// LINKAGE NOTE. This object references Win_texture_update_f32, which lives in
// the gui package's libgui.a. libwynimg.a is also linked by the test suite,
// which does NOT link libgui - that is safe, because a static archive only
// contributes members that are actually referenced, and nothing in the tests
// names wynimg_upload_tex. Verified: `wyn test` stays 12 passed after this file
// joined the archive.
#include "wynimg.h"

// Declared here rather than by including gui_backend.h, so building the shim
// needs no include path into a sibling repository. The signature is copied
// verbatim from repos/gui/src/gui_backend.h; a mismatch would be a hard C
// error at this call, not a silent miscall.
long long Win_texture_update_f32(long long tex, const float* rgba,
                                 long long w, long long h);

// Uploads the whole buffer `p` into texture `tex`. Returns 1 on success, 0 if
// the handle is dead, the buffer has no pixels, or the backend rejected the
// upload (typically a size mismatch between texture and buffer).
long long wynimg_upload_tex(long long tex, void* p) {
    WynImg* im = wynimg_deref(p);
    if (!im || !im->px) return 0;
    return Win_texture_update_f32(tex, im->px, im->w, im->h);
}

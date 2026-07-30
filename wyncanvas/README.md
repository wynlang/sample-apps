# WynCanvas

A layered image editor written in [Wyn](https://wynlang.com), with a C shim for
pixel work.

**Implemented:** float32 linear imaging core, non-destructive layer model,
27 blend modes, masks, adjustment layers, PNG I/O, undo/redo, a native SDL3
window, and **editing**: a soft-edged brush and eraser, a bucket fill, an
eyedropper, an HSV colour picker, working pan/zoom, and layer add / delete /
reorder / opacity / blend-mode / visibility - every one of them a single
undoable command.

**Not implemented:** selections, text, vectors, PSD, filters, transform tools,
layer groups. A layer's MASK is not captured by the delete/undo record (there is
no eleventh entry column for it), so undoing a delete restores the pixels and
properties but not the mask.

### What "the brush works" means here, and how it is checked

A paint stroke is **two buffers and one composite**, not a sequence of dabs:
`wynimg_stroke_seg` accumulates *coverage* with `max()` so overlap saturates
instead of accumulating, and `wynimg_stroke_apply` rebuilds the layer as
`base + colour x coverage` from a snapshot taken when the stroke began. Three
consequences, all required: a 30%-alpha stroke stays 30% where the pointer
dawdled; the live preview is produced by the identical code as the commit; and
the whole gesture is **one** undo entry rather than 400.

Coverage is a **distance field to the segment**, not a stamp at each mouse
position, so a fast drag is one uniform capsule rather than a dotted line.

The pixels are the test. `wyn test` covers the core (`tests/test_paint.wyn`
drives begin/seg/commit in document coordinates and reads channels back;
`tests/test_view.wyn` asserts the view transform is invertible at nine zooms and
five pans). `./verify_ui.sh` covers the **editor**: it drives the real binary
through real platform events and greps printed pixel values, including that a
stroke aimed at document (100,100) lands there at 100%, 400%-plus-pan, 800% and
25% zoom, and that the WINDOW framebuffer shows it at the mapped coordinates.

Tiling is **not** implemented. `WYNIMG_TILE = 256` is declared in
`csrc/wynimg.h` so the value lives in one place, but cycle 1 allocates
whole-image buffers; dirty-region invalidation and per-tile parallelism are
deferred.

## Requirements

- Wyn v1.20.0 (`~/.wyn/bin/wyn`)
- libpng, zlib (`brew install libpng`)
- A C compiler (`cc`)

## Build and run

```bash
export WYN_ROOT=/path/to/wynlang/wyn   # required: your wyn compiler checkout
./csrc/build.sh                                          # build the C shim
~/.wyn/bin/wyn test                                      # run all tests
~/.wyn/bin/wyn run src/ui.wyn                            # launch the editor

# headless verification of the editor itself (57 pixel assertions)
~/.wyn/bin/wyn build src/ui.wyn -o /tmp/wcui && ./verify_ui.sh /tmp/wcui
```

The editor needs `repos/gui`'s backend built too: `cd ../../gui && ./csrc/build.sh`.

### Driving the editor headlessly

`src/ui.wyn` reads three environment variables, which is how every claim in this
README is checked without a display:

| Variable | Effect |
|---|---|
| `WYNCANVAS_FRAMES=n` | run n frames and exit |
| `WYNCANVAS_SHOT=path` | write a BMP screenshot of the last frame |
| `WYNCANVAS_SCRIPT=a,b,c` | run actions against the real widgets and event queue |

Actions include `click:Label` (a real press+release through the platform queue),
`stroke:x0:y0:x1:y1` (a drag in DOCUMENT coordinates, mapped through the same
transform the blit uses), `px:x:y` (print a composited pixel), `screen:x:y`
(print the WINDOW framebuffer at where that document pixel maps), `fill:`,
`eyedrop:`, `pickat:`, `huedrag:`, `zoom:`, `pan:`, `map:`, `tool:`, `hsv:`,
`brush:`, `layers`, `undo`, `redo`, `save`, `open`.

```bash
SDL_VIDEODRIVER=dummy WYNCANVAS_FRAMES=2 \
  WYNCANVAS_SCRIPT='click:+L,hsv:0:1:1,brush:12:1:1,tool:brush,stroke:100:100:100:100,px:100:100' \
  /tmp/wcui
#   px(100,100)=1.000,0.000,0.000 a=1.000
```

Headless compositing:

```bash
~/.wyn/bin/wyn run src/cli.wyn -- bottom.png top.png out.png
```

Layers composite in argument order (first = bottom); the last argument is the
output. All inputs must share dimensions — a mismatch is an error, not a
silently-skipped layer. A missing or non-PNG input prints `error: cannot read
<path>` and exits 1.

**Do not build with `--release`.** It selects `wyn_runtime_slim.h`, which is
missing definitions the project needs; the current failure is
`Undefined symbols: _Math_pow, _System_args, ___wyn_argc`. The default (TCC)
path is the supported one and is fast.

## Architecture

```
Native window (App)  ──HTTP──►  Wyn: layers, history, render walk
                                     │ extern fn
                                     ▼
                                C: wynimg + libpng
```

Pixels live in C memory as flat `float*` and never enter Wyn in bulk: Wyn arrays
are 16-byte tagged unions, so a 12MP RGBA image would cost ~768MB and could not
be handed to libpng. Wyn holds opaque handles.

The UI talks to Wyn over localhost HTTP because the webview has no JS→Wyn IPC
(`wyn_webview.h` exposes only create/html/url/eval/run/destroy/set_title). A
spawned server keeps answering while `App.run()` blocks in `[NSApp run]`.

Pixels are **float32 RGBA, linear light, premultiplied**. sRGB conversion
happens only at I/O edges — compositing sRGB values directly produces visibly
wrong midtones.

## Gotchas

Each of these cost real debugging time. They are platform facts, not
preferences.

- `export WYN_ROOT=...` is required or package resolution fails.
- **FFI types:** Wyn `float` ⇄ C `double`, Wyn `int` ⇄ C `long long`. A C
  function declared `float` bound to Wyn `-> float` returns *garbage*, silently —
  measured `5.25e-315` for a `0.875f` return.
- **Handles are spelled `int`, never `ptr`.** `ptr` in a module `pub fn`
  signature emits the undefined C type `<module>_ptr`.
- The C shim must be a **static archive**; `[ffi]` accepts only `libs`,
  `lib_dirs`, `include_dirs` — there is no `objects` key.
- Homebrew libraries need an explicit `lib_dirs` entry, and libpng also needs `z`.
- **Call convention:** `extern fn`s are called BARE from importing modules
  (`wynimg_new(...)`); a `pub fn` that needs an extern must use `wynimg::new(...)`,
  because plain and dot-qualified spellings both get double-prefixed.
- Every module needs a **root-level symlink** (`./pixel.wyn -> src/pixel.wyn`);
  `import` does not search `src/`.
- Tests use `test "name" { }`. Only `assert_eq`, `assert`, `assert_true`,
  `assert_false` exist — there is **no `assert_eq_float`**; use `pixel.approx`.
- `Time.now_millis()`, not `now_ms`. Unknown namespace methods fall through to C
  as undeclared-function errors.
- **The Wyn runtime cannot serve binary data.** `File.read` opens mode `"r"` and
  NUL-terminates; `Http.respond` sizes the body with `strlen`. A PNG served that
  way is truncated at its first NUL. `csrc/wynimg_http.c` writes the response
  bytes directly instead.
- **Multi-line strings need `"""`**, and a literal `${` inside one must be
  written `\${` or Wyn interpolates it.
- **`spawn` does not work inside an imported module** — no wrapper is emitted.
  Keep spawn sites in the entry file (see `src/app.wyn`).
- **`wyn run f.wyn -- a b` leaks the `--`** into `System.args()` on the cached
  (no-recompile) path. `src/cli.wyn` skips a leading `--` defensively.
- A local `var` in a `test` block whose name matches a `pub fn` of any
  transitively-imported module is resolved as that function
  ("Cannot compare function with int"). Rename the variable.

Symptom / minimal-repro / root-cause records live with the upstream compiler issues.

## Layout

| Path | Purpose |
|---|---|
| `csrc/` | C shim: buffers, blend kernels, codecs, binary HTTP send |
| `src/pixel.wyn` | FFI bindings, sRGB, HSV, helpers |
| `src/paint.wyn` | Tools, brush parameters, the stroke lifecycle |
| `src/view.wyn` | The viewport transform: zoom, pan, both mappings |
| `src/layer.wyn` | Layer records |
| `src/render.wyn` | Compositor |
| `src/history.wyn` | Undo/redo |
| `src/server.wyn` | HTTP endpoints |
| `src/app.wyn` | Window bootstrap + accept loop |
| `src/cli.wyn` | Headless composite |
| `src/ui.wyn` | Native SDL3 editor window: layout, paint, dispatch |
| `verify_ui.sh` | 57 headless pixel assertions against the built editor |
| `tests/golden/` | Reference images (see its README) |

## Compiler bugs found while building the editor

Both were minimised to two-file repros and worked around locally, not patched in
the compiler.

**1. A local `var v = <float>` in an imported module retypes a same-named `int`
in the importer, and the resulting `.to_float()` silently yields 0.**

```wyn
// m.wyn
pub fn f() { var dx = 1.0 }

// t.wyn
import m
fn g(dx: int) -> float { return dx.to_float() + 0.5 }
fn main() -> int { print("${g(3)}") return 0 }
// prints 0.5, not 3.5 -- plus a NON-FATAL
// "Error: Unknown method 'to_float' for type 'float'"
```

This is worse than a compile error, because the build succeeds. It would have put
every scripted brush stroke at window (0,0). Worked around by renaming the
colliding locals: `src/paint.wyn` uses `chi`/`clo` rather than `mx`/`mn`, and
`src/ui.wyn`'s `win_x_of_doc` takes `docx` rather than `dx`.

**2. `from` is a reserved word and cannot be a parameter name.**

```wyn
fn g(from: int) -> int { return from + 1 }   // "Expected parameter name"
```

Renamed to `start` in `src/history.wyn`.

## Two real bugs this project's own tests caught

Recorded because both were invisible to the checks that existed before them.

**Entry columns drifted out of length.** `history.wyn`'s columns were extended
with four more for the pixel and structural commands, but the three scalar
records kept their own writer that appended to only the first six. A script that
clicked `b>` twice, `o-` twice, `dn`, `-L` and then Undo panicked with
`array index out of bounds: index 5, length 2`. A scalar-only session never
touches the new columns and never notices. Fixed by making `push` delegate to
`push_ex`, so ten columns are the same length by construction, and guarded by
"scalar records keep the extended columns in step".

**Deleting a layer lost its properties.** Deleting an `overlay` layer at 60%
opacity and undoing restored it as `normal` at 100%. The buffer came back, so the
image looked nearly right and the row read plausibly - silent data loss. Fixed by
capturing opacity, blend mode and visibility in the delete record's unused scalar
columns, and guarded by "deleting a layer captures its properties, not just its
pixels".

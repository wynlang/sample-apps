# WynCanvas

A layered image editor written in [Wyn](https://wynlang.com), with a C shim for
pixel work.

**Cycle 1 scope:** float32 linear imaging core, non-destructive layer model,
27 blend modes, masks, adjustment layers, PNG I/O, undo/redo, and a native
window. Brushes, selections, text, vectors, PSD and filters are cycles 2-7 —
see `docs/superpowers/specs/`.

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
~/.wyn/bin/wyn run src/app.wyn                           # launch the window
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

Full symptom / minimal-repro / root-cause records are in `WYN_FINDINGS.md`.

## Layout

| Path | Purpose |
|---|---|
| `csrc/` | C shim: buffers, blend kernels, codecs, binary HTTP send |
| `src/pixel.wyn` | FFI bindings, sRGB, helpers |
| `src/layer.wyn` | Layer records |
| `src/render.wyn` | Compositor |
| `src/history.wyn` | Undo/redo |
| `src/server.wyn` | HTTP endpoints |
| `src/app.wyn` | Window bootstrap + accept loop |
| `src/cli.wyn` | Headless composite |
| `tests/golden/` | Reference images (see its README) |

# Wyn Sample Apps

Real-world programs written in Wyn. Every app compiles and runs.

## Categories

### 🛠 cli-tools/ (7 apps)
| App | Description |
|-----|-------------|
| **loc** | Count lines of code, with per-language comment syntax |
| **todo-finder** | Scan for TODO/FIXME markers in comments, ranked by severity |
| **word-counter** | Word frequency analysis |
| **config-reader** | Parse a sectioned config and validate it against a schema |
| **md-toc** | Generate a Markdown TOC with GitHub-compatible anchors |
| **wyn-grep** | Search files for a pattern |
| **twux** | Terminal multiplexer *(interactive)* |

### 🖥 sysadmin/ (10 apps)
| App | Description |
|-----|-------------|
| **sysmon** | Terminal monitor — CPU/memory plus processes, disks or ports *(interactive)* |
| **sysinfo** | One system report: host, CPU, memory, disks, processes, network — and it says so when a metric is unavailable |
| **diskmon** | Disk usage with colour-coded bars, parsing POSIX `df -P` |
| **portscanner** | Concurrent port scan, closed vs filtered |
| **httpcheck** | Concurrent HTTP health checker with timing |
| **logwatch** | Log file analyzer with severity highlighting |
| **servicemon** | Service health monitor (`await_all`, not a sleep) |
| **envdiff** | Diff the environment against a baseline, redacting secrets |
| **dockermon** | Docker container monitor, tab-split so statuses survive |
| **gitdash** | Git dashboard — branch, divergence, conflicts, commits |

### 📊 data-processing/ (3 apps)
| App | Description |
|-----|-------------|
| **api-client** | HTTP + JSON + Base64 + Crypto + CSV + SQLite |
| **csv-analyzer** | Group and aggregate typed CSV columns |
| **log-parser** | Parse and report log errors |

### 🧮 algorithms/ (2 apps)
| App | Description |
|-----|-------------|
| **sorting** | Quicksort, mergesort, bubblesort |
| **fibonacci** | Recursive vs iterative comparison |

### 🌐 web/ (4 apps)

All four serve real HTTP and spawn a coroutine per request. They are kept apart because
each shows a different layer, not a different look: `curl localhost:8080/` returned
**byte-identical** HTML from all of them, so the distinction is the API underneath.

| App | What only this one shows |
|-----|--------------------------|
| **server** | the full stack: routing, SQLite, and both JSON serializers (`/api/info` pretty, `/api/info.min` compact) |
| **rest-api** | clean shutdown -- `Http.close_server` and `Db.close` |
| **template-demo** | `Template.render` against a real `templates/index.html` |
| **wyn-web-demo** | the `Web.*` router (`Web.get`/`post`/`match`) instead of hand-parsing paths |

### 📱 mobile/ (1 app)
| App | Description |
|-----|-------------|
| **counter** | Counter with undo — state transitions kept separate from the UI, so they are testable without a device (the SDK entry points are stubs) |

### 🎨 gui/ (5 apps, require SDL3)
| App | Description |
|-----|-------------|
| **hello_gui** | Colored rectangles and text |
| **dashboard** | System monitor with widgets |
| **pong** | Classic Pong game |
| **snake** | Classic Snake game |
| **notepad** | Text editor with input widget |

### 🖼 wyncanvas/ (1 app, requires a C toolchain)
The largest program here. A layered
image editor: float32 linear premultiplied imaging core, 27 blend modes, masks,
adjustment layers, PNG I/O, and undo/redo. Wyn for all logic, a thin C shim for
per-pixel work.

| Part | What |
|------|------|
| `src/pixel.wyn` | buffers, colour space, sRGB transfer |
| `src/layer.wyn` | layer stack, masks, adjustment layers |
| `src/render.wyn` | compositor, 27 blend modes |
| `src/history.wyn` | undo/redo as command records |
| `src/cli.wyn` | headless CLI |
| `csrc/` | C shim (`libwynimg`) for the hot pixel loops |
| `tests/` | 12 files, 149 tests, incl. a golden-image test |

```bash
cd wyncanvas
./csrc/build.sh                                    # build the C shim first
WYN_ROOT=/path/to/wyn /path/to/wyn/wyn test        # 12 files, 149 tests
```

## Running

```bash
cd wyn

# CLI and sysadmin tools
./wyn run ../sample-apps/sysadmin/sysinfo/src/main.wyn
./wyn run ../sample-apps/cli-tools/loc/src/main.wyn

# Interactive TUI apps
./wyn run ../sample-apps/sysadmin/sysmon/src/main.wyn
./wyn run ../sample-apps/cli-tools/twux/src/main.wyn

# GUI apps (requires: brew install sdl3)
./wyn run ../sample-apps/gui/pong/src/main.wyn
./wyn run ../sample-apps/gui/dashboard/src/main.wyn

# Web server
./wyn run ../sample-apps/web/rest-api/src/main.wyn
```

### python/
- **math-lib/** - Math library compiled to shared library with auto-generated Python wrapper (31 tests)

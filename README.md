# Wyn Sample Apps

Real-world programs written in Wyn. Every app compiles and runs.

## Categories

### 🛠 cli-tools/ (6 apps)
| App | Description |
|-----|-------------|
| **loc** | Count lines of code |
| **todo-finder** | Scan for TODO/FIXME comments |
| **word-counter** | Word frequency analysis |
| **config-reader** | Parse key=value config files |
| **md-toc** | Generate Markdown TOC |
| **twux** | Terminal multiplexer *(interactive)* |

### 🖥 sysadmin/ (13 apps)
| App | Description |
|-----|-------------|
| **sysmon** | System monitor - htop-lite *(interactive)* |
| **sysinfo** | System dashboard (CPU, memory, disk, network) |
| **procwatch** | Process monitor with CPU/memory sorting |
| **diskmon** | Disk usage with color-coded progress bars |
| **netstat-lite** | Network interfaces, ports, connections |
| **portscanner** | Scan common ports |
| **httpcheck** | HTTP health checker with timing |
| **logwatch** | Log file analyzer with severity highlighting |
| **servicemon** | Service health monitor |
| **envdiff** | Environment variable inspector |
| **filebrowser** | File browser TUI *(interactive)* |
| **dockermon** | Docker container monitor |
| **gitdash** | Git repository dashboard |

### 📊 data-processing/ (3 apps)
| App | Description |
|-----|-------------|
| **api-client** | HTTP + JSON + Base64 + Crypto + CSV + SQLite |
| **csv-analyzer** | Student grade statistics |
| **log-parser** | Parse and report log errors |

### 🧮 algorithms/ (2 apps)
| App | Description |
|-----|-------------|
| **sorting** | Quicksort, mergesort, bubblesort |
| **fibonacci** | Recursive vs iterative comparison |

### 🌐 web/ (3 apps)
| App | Description |
|-----|-------------|
| **website** | Web server with HTML templating and JSON API |
| **server** | Full web server with routing, SQLite, JSON API, arena GC |
| **rest-api** | REST API server with SQLite + JSON |

### 🎨 gui/ (4 apps, require SDL2)
| App | Description |
|-----|-------------|
| **hello_gui** | Colored rectangles and text |
| **dashboard** | System monitor with widgets |
| **pong** | Classic Pong game |
| **notepad** | Text editor with input widget |

## Running

```bash
cd wyn

# CLI and sysadmin tools
./wyn run ../sample-apps/sysadmin/sysinfo/main.wyn
./wyn run ../sample-apps/cli-tools/loc/main.wyn

# Interactive TUI apps
./wyn run ../sample-apps/sysadmin/sysmon/main.wyn
./wyn run ../sample-apps/cli-tools/twux/main.wyn

# GUI apps (requires: brew install sdl2)
./wyn run ../sample-apps/gui/pong/main.wyn
./wyn run ../sample-apps/gui/dashboard/main.wyn

# Web server
./wyn run ../sample-apps/web/rest-api/main.wyn
```

### python/
- **math-lib/** - Math library compiled to shared library with auto-generated Python wrapper (31 tests)

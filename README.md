# Wyn Sample Apps

Real-world programs written in [Wyn](https://github.com/AO-Design-Inc/wyn), demonstrating what you can build today.

Every app in this repo compiles and runs. If it's here, it works.

## Categories

### 🛠 cli-tools/
Command-line utilities that solve real problems.

| App | Description | Features |
|-----|-------------|----------|
| **loc** | Count lines of code | File I/O, structs, string parsing |
| **todo-finder** | Scan for TODO/FIXME comments | File I/O, string methods |
| **word-counter** | Word frequency analysis | HashMap, string splitting |
| **config-reader** | Parse key=value config files | HashMap, error handling |
| **md-toc** | Generate Markdown TOC | String pattern detection |
| **twux** | Terminal multiplexer *(interactive)* | Terminal TUI, panes, keyboard input |

### 🖥 sysadmin/
Real tools for DevOps and systems administration.

| App | Description | Features |
|-----|-------------|----------|
| **sysmon** | System monitor — htop-lite *(interactive)* | Terminal TUI, CPU/memory bars, process list |
| **sysinfo** | System dashboard | CPU, memory (64-bit), disk, network, env |
| **procwatch** | Process monitor | CPU/memory sorting, formatted output |
| **diskmon** | Disk usage with progress bars | Color-coded bars, POSIX df parsing |
| **netstat-lite** | Network status | Interfaces, ports, connections, DNS |
| **portscanner** | Scan common ports | Network probing, service names |
| **httpcheck** | HTTP health checker | curl, response timing, status codes |
| **logwatch** | Log file analyzer | Severity highlighting, health assessment |
| **servicemon** | Service health monitor | System/dev/Docker/Git status checks |
| **envdiff** | Environment inspector | PATH analysis, variable audit |
| **filebrowser** | File browser TUI *(interactive)* | Directory navigation, j/k/arrows |
| **dockermon** | Docker container monitor | Global variables, container/disk info |
| **gitdash** | Git repository dashboard | Global variables, branch/commit/stash |

### 📊 data-processing/
Programs that read, transform, and output data.

| App | Description | Features |
|-----|-------------|----------|
| **csv-analyzer** | Student grade statistics | CSV parsing, computed fields |
| **log-parser** | Parse and report log errors | String parsing, structs |

### 🧮 algorithms/
Classic algorithms implemented in Wyn.

| App | Description | Features |
|-----|-------------|----------|
| **sorting** | Quicksort, mergesort, bubblesort | Recursion, array manipulation |
| **fibonacci** | Recursive vs iterative comparison | Performance comparison |

## Running

```bash
# From the wyn/ directory
cd wyn

# Non-interactive tools
./wyn run ../sample-apps/sysadmin/sysinfo/main.wyn
./wyn run ../sample-apps/sysadmin/diskmon/main.wyn
./wyn run ../sample-apps/sysadmin/procwatch/main.wyn
./wyn run ../sample-apps/sysadmin/gitdash/main.wyn
./wyn run ../sample-apps/cli-tools/loc/main.wyn

# Interactive TUI apps (run in your terminal)
./wyn run ../sample-apps/sysadmin/sysmon/main.wyn
./wyn run ../sample-apps/cli-tools/twux/main.wyn
./wyn run ../sample-apps/sysadmin/filebrowser/main.wyn
```

## Requirements

- Wyn compiler (built from `wyn/` directory)
- No external dependencies

## Contributing

Each app should:
1. Compile and run without errors
2. Demonstrate real Wyn features
3. Be self-contained (no external dependencies)

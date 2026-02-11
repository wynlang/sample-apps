# Wyn Sample Apps

Real-world programs written in [Wyn](https://github.com/AO-Design-Inc/wyn), demonstrating what you can build today.

Every app in this repo compiles and runs. If it's here, it works.

## Categories

### cli-tools/
Command-line utilities that solve real problems.

- **loc** — Count lines of code across a project
- **todo-finder** — Scan source files for TODO/FIXME comments
- **word-counter** — Analyze word frequency in text files
- **config-reader** — Parse key=value config files with error handling
- **md-toc** — Generate table of contents from Markdown headings
- **twux** — Terminal multiplexer with split panes *(interactive — run manually)*

### data-processing/
Programs that read, transform, and output data.

- **csv-analyzer** — Read CSV data, compute statistics, output summary
- **log-parser** — Parse log files, extract errors, generate report

### algorithms/
Classic algorithms implemented in Wyn.

- **sorting** — Quicksort, mergesort, bubblesort with benchmarks
- **fibonacci** — Recursive and iterative with performance comparison

## Running

```bash
# From the wyn/ directory
cd wyn
./wyn run ../sample-apps/cli-tools/loc/main.wyn
./wyn run ../sample-apps/algorithms/sorting/main.wyn

# Interactive apps (run in your terminal, not from scripts)
./wyn run ../sample-apps/cli-tools/twux/main.wyn
```

## Requirements

- Wyn compiler (built from `wyn/` directory)
- No external dependencies

## Contributing

Each app should:
1. Compile and run without errors
2. Have a clear purpose described in its own README
3. Demonstrate real Wyn features (structs, methods, Result/Option, modules, etc.)
4. Be self-contained (no external dependencies)

# wyn-grep

Fast file search tool written in Wyn. Searches files recursively for regex patterns with colored output.

## Build

```bash
cd wyn
./wyn build ../sample-apps/cli-tools/wyn-grep/main.wyn
```

## Usage

```bash
wyn-grep <pattern> [path] [-n]

# Search for TODO in current directory
wyn-grep TODO .

# Search with line numbers
wyn-grep "fn main" src -n

# Regex patterns
wyn-grep "error|warn" /var/log
```

## Features

- Recursive file search
- POSIX Extended Regex (ERE) patterns
- Colored output (file paths in purple, line numbers in green)
- Auto-skips binary files, .git, node_modules, build artifacts
- ~90 lines of Wyn

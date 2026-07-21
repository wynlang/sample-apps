# loc - Lines of Code Counter

A command-line tool that counts lines of code in source files.

## Usage

```bash
cd wyn
./wyn run ../sample-apps/cli-tools/loc/main.wyn
```

## Features

- Counts total lines, code lines, blank lines, and comment lines
- Processes `.wyn` files in the current directory
- Displays per-file and summary statistics

## Wyn Features Demonstrated

- Structs with methods (`FileStats.total()`)
- Result type for error handling
- String operations (`str_starts_with`, `str_trim`)
- File I/O (`file_read`)
- Arrays and iteration
- Integer-to-string conversion

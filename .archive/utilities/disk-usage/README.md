# Disk Usage Analyzer

A simple ncdu (NCurses Disk Usage) clone written in Wyn.

## Features

- Recursive directory scanning
- File and directory size calculation
- Simple text output

## Usage

```bash
wyn run main.wyn [directory]
```

## Example

```bash
# Analyze current directory
wyn run main.wyn

# Analyze specific directory
wyn run main.wyn /tmp
```

## Future Enhancements

- Interactive TUI (requires terminal library)
- Sorting by size
- Filtering options
- Progress indicator

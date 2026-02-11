# Log Analyzer

Analyze log files with pattern detection and automatic report generation.

## Features

- Real file I/O with `File::read()` and `File::write()`
- Pattern detection using OO string methods
- System integration with `System::exec()`
- Automatic report generation

## Usage

```bash
# Install Wyn compiler first
git clone https://github.com/wyn-lang/wyn.git
cd wyn && make
export PATH=$PATH:$(pwd)

# Clone and run
cd ..
git clone https://github.com/wyn-lang/sample-apps.git
cd sample-apps/log-analyzer

# Compile and run
wyn main.wyn
./main.wyn.out
```

## Example Output

```
=== LOG ANALYZER ===

File size: 435 bytes

Analysis Results:
  ✗ Errors detected
  ⚠ Warnings detected
  • Database issues found

Total lines: 8

Report saved to /tmp/log_report.txt
```

## What It Demonstrates

- `File::read()` - Read log files
- `File::write()` - Generate reports
- `System::exec()` - Execute system commands
- `.contains()` - Pattern matching with OO methods
- `.trim()` - String manipulation
- `.len()` - Get string length

All stdlib modules available without import!

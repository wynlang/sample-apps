# Wyn Sample Applications

Real-world applications demonstrating Wyn v1.4.0 features and best practices.

## Applications

### 1. calculator-modules ⭐ NEW
Demonstrates Wyn's module system with a calculator.

**Features:**
- Module imports (`import calculator`)
- Namespaced function calls
- Type-safe module boundaries
- Multiple module usage

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/calculator-modules/main.wyn
../sample-apps/calculator-modules/main.wyn.out
```

### 2. file-finder
Find files by extension with size statistics.

**Features:**
- Command-line arguments (System::args)
- Directory listing (File::list_dir)
- File type checking (File::is_file)
- File size calculation (File::file_size)
- String methods (ends_with)

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/file-finder/main.wyn
../sample-apps/file-finder/main.wyn.out .wyn
```

### 3. code-stats
Analyze source code statistics in a directory.

**Features:**
- Real line counting by reading files
- Multiple file type support (.wyn, .c, .h)
- Statistical calculations (totals, averages)
- Report generation
- Path manipulation (File::path_join)

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/code-stats/main.wyn
../sample-apps/code-stats/main.wyn.out examples
```

### 4. build-monitor
Monitor directory for Wyn source files with change detection.

**Features:**
- Directory monitoring
- Time-based scanning (Time::now, Time::sleep)
- Change detection
- Real-time file listing

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/build-monitor/main.wyn
../sample-apps/build-monitor/main.wyn.out examples
```

### 5. csv-processor
Parse and analyze CSV data files.

**Features:**
- CSV file creation and reading
- Data validation
- System command integration (grep, wc)
- String operations (contains, trim)
- Report generation

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/csv-processor/main.wyn
../sample-apps/csv-processor/main.wyn.out
```

### 6. log-analyzer
Analyze log files for errors, warnings, and patterns.

**Features:**
- Log file parsing
- Pattern detection (ERROR, WARN)
- String analysis (contains, len)
- Report generation
- Real file I/O

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/log-analyzer/main.wyn
../sample-apps/log-analyzer/main.wyn.out
```

### 7. disk-analyzer
Analyze disk usage and file statistics.

**Features:**
- System command integration (du, find)
- Real disk usage calculation
- File and directory counting
- Large file detection
- Report generation

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/disk-analyzer/main.wyn
../sample-apps/disk-analyzer/main.wyn.out
```

### 8. process-monitor
Monitor system processes and resources.

**Features:**
- Real process listing (ps)
- Memory status (free/vm_stat)
- System uptime
- Process detection
- Cross-platform commands

**Usage:**
```bash
cd wyn
./wyn ../sample-apps/process-monitor/main.wyn
../sample-apps/process-monitor/main.wyn.out
```

## Building All Apps

```bash
cd wyn
for app in ../sample-apps/*/main.wyn; do
    echo "Building $(dirname $app)..."
    ./wyn "$app"
done
```

## Key Features Demonstrated

- **File I/O:** Read, write, list directories, check file types
- **System Integration:** Execute commands, get environment, arguments
- **Time Operations:** Timestamps, delays, monitoring
- **String Processing:** Parsing, searching, manipulation
- **Real Calculations:** Statistics, totals, averages
- **Report Generation:** Creating output files with results

## Design Principles

All sample apps follow Wyn v1.4.0 best practices:

1. **Pure OO API:** Methods (`.method()`) and modules (`Module::function()`)
2. **String Interpolation:** Clean output with `"text ${variable}"` syntax
3. **Modern Iteration:** `for i in 0..n` instead of while loops
4. **Optional Parentheses:** `if x > 0 { }` instead of `if (x > 0) { }`
5. **Real Functionality:** No fake data or hardcoded values
6. **Type Safety:** Proper type inference and checking
7. **Error Handling:** Graceful failure with helpful messages

## Testing

All apps are tested in the regression suite:

```bash
cd wyn
./tests/regression.sh
```

## Version

These apps are built for **Wyn v1.4.0** and demonstrate production-ready features.

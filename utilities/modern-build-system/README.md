# Modern Build System

A comprehensive build system showcasing all Wyn language features.

## Features

- **File Operations**: Recursive directory scanning, file timestamps
- **Process Execution**: Compile commands, parallel builds
- **Time API**: Build timestamps, duration tracking
- **Environment Variables**: Build configuration
- **Collections**: Dependency tracking with Queue
- **String Comparison**: File extension matching
- **CLI Arguments**: Build flags and options
- **Error Handling**: Option types for missing files

## Usage

```bash
# Build all files
./build-system

# Clean build
./build-system --clean

# Verbose output
./build-system --verbose

# Parallel build
./build-system --jobs=4
```

## What It Demonstrates

1. **File System API** - Scanning directories, checking timestamps
2. **Process Execution** - Running compiler commands
3. **Time API** - Tracking build duration
4. **Environment** - Reading build configuration
5. **Collections** - Queue for build tasks
6. **String Comparison** - Matching file extensions
7. **CLI Parsing** - Command line options
8. **Error Handling** - Graceful failure handling

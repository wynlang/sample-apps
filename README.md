# Wyn Sample Applications

Production-ready applications showcasing Wyn v1.5.0 features.

## 🚀 What's New in v1.5.0

All sample apps can now leverage:
- ✅ **Enums with data** - Type-safe states and results
- ✅ **Pattern matching** - Elegant control flow
- ✅ **Generic HashMap<K,V>** - Type-safe collections
- ✅ **Option/Result types** - Safe error handling
- ✅ **String indexing** - Direct character access
- ✅ **Type inference** - Less boilerplate
- ✅ **Variable HashMap keys** - Dynamic lookups

**⚠️ Note:** v1.5.0 uses underscore syntax (`Result_Ok`). **v1.6.0 adds namespace operator (`Result::Ok`)** for cleaner code.

**See [V1.5_MIGRATION.md](V1.5_MIGRATION.md) for v1.5.0 upgrade guide**
**See [V1.6_FEATURES.md](V1.6_FEATURES.md) for v1.6.0 improvements**

## 🌟 Featured: v1.5.0 Showcase

**The ultimate demonstration of Wyn's modern type system!**

```bash
cd wyn
./wyn ../sample-apps/showcase-v1.5/main.wyn
../sample-apps/showcase-v1.5/main.wyn.out
```

Features demonstrated:
- ✅ Enums with associated data (Result, Option)
- ✅ Pattern matching with destructuring
- ✅ Generic collections (HashMap<K,V>)
- ✅ Type inference through function returns
- ✅ String indexing with [] syntax
- ✅ Variable HashMap keys
- ✅ Option/Result methods (unwrap, is_some, etc.)
- ✅ Bool methods (to_string, to_int)

**Beautiful syntax + Complex functionality = Wyn v1.5.0** 🚀

---

## Categories

- **🌟 Showcase** - v1.5.0 feature demonstration
- **Data Processing** - Log analysis, CSV processing, text manipulation
- **Networking** - HTTP clients, TCP communication
- **Web Apps** - Web servers, HTTP services
- **Utilities** - File finders, disk analyzers, process monitors
- **Dev Tools** - Code statistics, build monitors
- **Tutorials** - Learning examples with modules

---

## How v1.5.0 Improves Each Category

### Data Processing Apps
**Can now use:**
- Result enums for validation errors
- Pattern matching for data classification
- HashMap for statistics and metrics
- Option for safe lookups
- String indexing for parsing

**Example upgrade:**
```wyn
// Before: Error codes
fn validate(data: string) -> int { return -1 }

// After: Type-safe Result
enum Result { Success(int), Error(string) }
fn validate(data: string) -> Result {
    return Result_Error("Invalid")
}
```

### Networking Apps
**Can now use:**
- HttpResult enum for responses
- Pattern matching for status codes
- HashMap for headers
- Option for optional fields

### Web Apps
**Can now use:**
- Route enum for endpoints
- Pattern matching for request handling
- HashMap for route mapping
- Result for response generation

### Utilities
**Can now use:**
- SearchResult enum for file operations
- Option for file lookups
- HashMap for metadata
- Pattern matching for file types

---

## Data Processing Apps

### 1. Data Pipeline (`data-processing/data-pipeline/`)
**Comprehensive feature demonstration**

Features:
- ✅ Functional programming (.map, .filter, .reduce)
- ✅ Async/await operations
- ✅ Higher-order functions
- ✅ String processing (40+ methods)
- ✅ File I/O operations
- ✅ Integer/Float methods
- ✅ System operations

```bash
cd wyn
./wyn ../sample-apps/data-processing/data-pipeline/main.wyn
../sample-apps/data-processing/data-pipeline/main.wyn.out
```

### 2. Log Analyzer (`data-processing/log-analyzer/`)
**Real-world log file analysis**

Features:
- ✅ Async file operations
- ✅ String searching and parsing
- ✅ Functional analysis with higher-order functions
- ✅ Report generation
- ✅ File I/O

### 3. CSV Processor (`data-processing/csv-processor/`)
**Structured data processing**

Features:
- ✅ CSV parsing
- ✅ String splitting and trimming
- ✅ Array operations
- ✅ Data transformation

### 4. Text Processor (`data-processing/text-processor/`)
**Advanced text manipulation**

Features:
- ✅ Module system
- ✅ String utilities
- ✅ File operations
- ✅ Text transformations

---

## Networking Apps

### 5. HTTP Client (`networking/http-client/`)
**TCP networking and HTTP protocol**

Features:
- ✅ TCP networking (Net::connect, send, recv)
- ✅ Async/await for I/O
- ✅ String manipulation
- ✅ HTTP protocol handling

```bash
cd wyn
./wyn ../sample-apps/networking/http-client/main.wyn
../sample-apps/networking/http-client/main.wyn.out
```

---

## Web Apps

### 6. Web Server (`web-apps/web-server/`)
**HTTP web server with TCP sockets**

Features:
- ✅ TCP socket server (Net::listen)
- ✅ Async request handling
- ✅ HTTP response generation
- ✅ HTML content serving
- ✅ Real web server functionality

```bash
cd wyn
./wyn ../sample-apps/web-apps/web-server/main.wyn
../sample-apps/web-apps/web-server/main.wyn.out
# Visit: http://localhost:8080
```

---

## Utilities

### 7. File Finder (`utilities/file-finder/`)
**Fast file search utility**

Features:
- ✅ Recursive directory traversal
- ✅ Pattern matching
- ✅ File system operations

### 8. Disk Analyzer (`utilities/disk-analyzer/`)
**Disk usage analysis**

Features:
- ✅ Directory scanning
- ✅ Size calculations
- ✅ System commands

### 9. Process Monitor (`utilities/process-monitor/`)
**System process monitoring**

Features:
- ✅ System::exec integration
- ✅ Process listing
- ✅ Real-time monitoring

### 10. Build Monitor (`utilities/build-monitor/`)
**Build system integration**

Features:
- ✅ File watching
- ✅ Command execution
- ✅ Status reporting

---

## Dev Tools

### 11. Code Stats (`dev-tools/code-stats/`)
**Code metrics and analysis**

Features:
- ✅ File parsing
- ✅ Line counting
- ✅ Statistics generation

---

## Tutorials

### 12. Calculator with Modules (`tutorials/calculator-modules/`)
**Module system demonstration**

Features:
- ✅ Module imports
- ✅ State management
- ✅ History tracking
- ✅ Statistics

---

## Running All Apps

**Important:** The Wyn compiler must be run from the `wyn/` directory due to runtime dependencies.

```bash
# Test all apps compile
cd wyn
../sample-apps/test.sh

# Compile individual apps
cd wyn
./wyn ../sample-apps/data-processing/data-pipeline/main.wyn
./wyn ../sample-apps/networking/http-client/main.wyn
./wyn ../sample-apps/web-apps/web-server/main.wyn

# Run compiled apps
../sample-apps/data-processing/data-pipeline/main.wyn.out
../sample-apps/networking/http-client/main.wyn.out
../sample-apps/web-apps/web-server/main.wyn.out
```

---

## Features Demonstrated

### v1.5.0 Modern Type System ⭐ NEW
- ✅ **Enums with data**: `enum Result { Ok(int), Err(string) }`
- ✅ **Pattern matching**: `match result { Result_Ok(x) => x, ... }`
- ✅ **Generic HashMap**: `HashMap<string, int>` with type safety
- ✅ **Type inference**: `var map = create_map()` (type inferred!)
- ✅ **String indexing**: `text[0]` returns character
- ✅ **Variable HashMap keys**: `map[variable] = value`
- ✅ **Option methods**: `unwrap()`, `is_some()`, `unwrap_or()`
- ✅ **Result methods**: `unwrap()`, `is_ok()`, `unwrap_or()`
- ✅ **Bool methods**: `to_string()`, `to_int()`

### How to Upgrade Your Apps
All existing v1.4.0 apps continue to work. To use v1.5.0 features:

1. **Replace error codes** with Result enums
2. **Add pattern matching** for cleaner control flow
3. **Use HashMap** for key-value data
4. **Add Option** for nullable values
5. **Use string indexing** for character access

See [V1.5_MIGRATION.md](V1.5_MIGRATION.md) for detailed examples.

### Language Features
- ✅ Function types: `fn(T) -> R`
- ✅ Async/await: `async fn`, `await`
- ✅ Higher-order functions
- ✅ Module system
- ✅ Type safety

### Functional Programming
- ✅ `.map()` - Transform elements
- ✅ `.filter()` - Select elements
- ✅ `.reduce()` - Aggregate values
- ✅ Function composition

### Standard Library (114+ methods)
- ✅ String (40+): trim, upper, contains, split, etc.
- ✅ Array (21+): len, map, filter, reduce, etc.
- ✅ Integer (14+): to_string, to_binary, is_even, etc.
- ✅ Float (15+): round, floor, sqrt, sin, etc.
- ✅ File (10): read, write, exists, delete, etc.
- ✅ System (6): exec, args, env, etc.
- ✅ Time (3): now, sleep, format
- ✅ Net (5): listen, connect, send, recv, close

### Real-World Patterns
- ✅ Data pipelines
- ✅ Async I/O
- ✅ Network clients
- ✅ Web servers
- ✅ File processing
- ✅ System integration
- ✅ Error handling

---

## Requirements

- Wyn v1.5.0 or later
- macOS, Linux, or Windows
- GCC or Clang compiler

---

## Status

All applications are:
- ✅ Production-ready
- ✅ Fully tested
- ✅ Well-documented
- ✅ Feature-complete

**Total:** 13 sample applications organized by category.

**Latest:** v1.5.0 Showcase demonstrates modern type system with beautiful syntax!

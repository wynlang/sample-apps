# Wyn Sample Applications

Production-ready applications and tutorials demonstrating Wyn v1.4.0 features.

---

## 📁 Directory Structure

```
sample-apps/
├── utilities/          # Real-world CLI tools (4 apps)
├── data-processing/    # Data manipulation apps (3 apps)
├── dev-tools/          # Developer utilities (1 app)
├── tutorials/          # Learning examples (1 app)
├── web-apps/           # Web servers and APIs (coming soon)
├── games/              # Interactive games (coming soon)
├── networking/         # Network protocols and tools (coming soon)
└── gui/                # Graphical applications (coming soon)
```

---

## 🛠️ Utilities (4 apps)

Real-world command-line tools for system administration and monitoring.

### file-finder
Find files by extension with size statistics.
```bash
cd wyn
./wyn ../sample-apps/utilities/file-finder/main.wyn
../sample-apps/utilities/file-finder/main.wyn.out .wyn
```

### disk-analyzer
Analyze disk usage and file statistics.
```bash
cd wyn
./wyn ../sample-apps/utilities/disk-analyzer/main.wyn
../sample-apps/utilities/disk-analyzer/main.wyn.out
```

### process-monitor
Monitor system processes and resources.
```bash
cd wyn
./wyn ../sample-apps/utilities/process-monitor/main.wyn
../sample-apps/utilities/process-monitor/main.wyn.out
```

### build-monitor
Monitor directory for source file changes.
```bash
cd wyn
./wyn ../sample-apps/utilities/build-monitor/main.wyn
../sample-apps/utilities/build-monitor/main.wyn.out examples
```

---

## 📊 Data Processing (3 apps)

Applications for parsing, analyzing, and transforming data.

### log-analyzer
Analyze log files for errors, warnings, and patterns.
```bash
cd wyn
./wyn ../sample-apps/data-processing/log-analyzer/main.wyn
../sample-apps/data-processing/log-analyzer/main.wyn.out
```

### csv-processor
Parse and analyze CSV data files.
```bash
cd wyn
./wyn ../sample-apps/data-processing/csv-processor/main.wyn
../sample-apps/data-processing/csv-processor/main.wyn.out
```

### text-processor
Multi-module text processing application.
```bash
cd wyn
./wyn ../sample-apps/data-processing/text-processor/main.wyn
../sample-apps/data-processing/text-processor/main.wyn.out
```

---

## 💻 Dev Tools (1 app)

Developer productivity tools for code analysis.

### code-stats
Analyze source code statistics in a directory.
```bash
cd wyn
./wyn ../sample-apps/dev-tools/code-stats/main.wyn
../sample-apps/dev-tools/code-stats/main.wyn.out examples
```

---

## 🎓 Tutorials (1 app)

Learning examples demonstrating language features.

### calculator-modules
Demonstrates Wyn's module system with imports.
```bash
cd wyn
./wyn ../sample-apps/tutorials/calculator-modules/main.wyn
../sample-apps/tutorials/calculator-modules/main.wyn.out
```

---

## 🚀 Quick Start

### Build All Apps
```bash
cd wyn
for app in ../sample-apps/{utilities,data-processing,dev-tools,tutorials}/*/main.wyn; do
    echo "Building $app..."
    ./wyn "$app"
done
```

### Run Tests
```bash
cd sample-apps
./test.sh
```

---

## 📚 Key Features Demonstrated

- **File I/O:** Read, write, list directories, check file types
- **System Integration:** Execute commands, get environment, arguments
- **Time Operations:** Timestamps, delays, monitoring
- **String Processing:** Parsing, searching, manipulation
- **Module System:** Multi-file projects with imports
- **Real Calculations:** Statistics, totals, averages
- **Report Generation:** Creating output files with results

---

## 🎯 Design Principles

All sample apps follow Wyn v1.4.0 best practices:

1. **Pure OO API:** Methods (`.method()`) and modules (`Module::function()`)
2. **String Interpolation:** Clean output with `"text ${variable}"` syntax
3. **Modern Iteration:** `for i in 0..n` instead of while loops
4. **Optional Parentheses:** `if x > 0 { }` instead of `if (x > 0) { }`
5. **Real Functionality:** No fake data or hardcoded values
6. **Type Safety:** Proper type inference and checking
7. **Error Handling:** Graceful failure with helpful messages

---

## 📦 Adding New Apps

When adding new applications:

1. **Choose the right category:**
   - `utilities/` - CLI tools for system tasks
   - `data-processing/` - Data transformation/analysis
   - `dev-tools/` - Developer productivity tools
   - `tutorials/` - Feature demonstrations

2. **Create new categories as needed:**
   - `web-apps/` - Web servers and APIs
   - `games/` - Interactive applications
   - `networking/` - Network tools
   - `gui/` - Graphical applications

3. **Follow the structure:**
   ```
   category/app-name/
   ├── main.wyn
   ├── README.md (optional)
   └── *.wyn (additional modules)
   ```

---

## 📄 Version

Built for **Wyn v1.4.0** - Production Ready ✓

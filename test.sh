#!/bin/bash
# Test all sample applications

set -e

echo "=== Testing Wyn Sample Applications ==="
echo ""

# Check if wyn is available
if ! command -v wyn &> /dev/null; then
    echo "Error: 'wyn' compiler not found in PATH"
    echo "Install from: https://github.com/wyn-lang/wyn"
    exit 1
fi

cd "$(dirname "$0")"

# Test log analyzer
echo "1. Testing Log Analyzer..."
cd log-analyzer
wyn main.wyn > /dev/null 2>&1
./main.wyn.out > /dev/null
echo "   ✓ Log Analyzer works"
cd ..

# Test process monitor
echo "2. Testing Process Monitor..."
cd process-monitor
wyn main.wyn > /dev/null 2>&1
./main.wyn.out > /dev/null
echo "   ✓ Process Monitor works"
cd ..

# Test CSV processor
echo "3. Testing CSV Processor..."
cd csv-processor
wyn main.wyn > /dev/null 2>&1
./main.wyn.out > /dev/null
echo "   ✓ CSV Processor works"
cd ..

# Test disk analyzer
echo "4. Testing Disk Analyzer..."
cd disk-analyzer
wyn main.wyn > /dev/null 2>&1
./main.wyn.out > /dev/null
echo "   ✓ Disk Analyzer works"
cd ..

echo ""
echo "=== All Sample Apps Passed! ==="

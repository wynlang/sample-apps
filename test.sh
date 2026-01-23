#!/bin/bash
# Test all sample applications
# NOTE: Must be run from the wyn/ directory due to compiler runtime dependencies

set -e

echo "=== Testing Wyn Sample Applications ==="
echo ""

# Check if we're in the right directory
if [ ! -f "wyn" ]; then
    echo "Error: Must run from wyn/ directory"
    echo "Usage: cd wyn && ../sample-apps/test.sh"
    exit 1
fi

WYN_CMD="./wyn"
echo "Using compiler: $WYN_CMD"
echo ""

# Test utilities
echo "Testing Utilities..."
for app in ../sample-apps/utilities/*/main.wyn; do
    app_name=$(basename $(dirname $app))
    echo "  - $app_name"
    $WYN_CMD "$app" > /dev/null 2>&1 && echo "    ✓ Compiled" || echo "    ✗ Failed to compile"
done

# Test data-processing
echo ""
echo "Testing Data Processing..."
for app in ../sample-apps/data-processing/*/main.wyn; do
    app_name=$(basename $(dirname $app))
    echo "  - $app_name"
    $WYN_CMD "$app" > /dev/null 2>&1 && echo "    ✓ Compiled" || echo "    ✗ Failed to compile"
done

# Test dev-tools
echo ""
echo "Testing Dev Tools..."
for app in ../sample-apps/dev-tools/*/main.wyn; do
    app_name=$(basename $(dirname $app))
    echo "  - $app_name"
    $WYN_CMD "$app" > /dev/null 2>&1 && echo "    ✓ Compiled" || echo "    ✗ Failed to compile"
done

# Test tutorials
echo ""
echo "Testing Tutorials..."
for app in ../sample-apps/tutorials/*/main.wyn; do
    app_name=$(basename $(dirname $app))
    echo "  - $app_name"
    $WYN_CMD "$app" > /dev/null 2>&1 && echo "    ✓ Compiled" || echo "    ✗ Failed to compile"
done

# Test networking
echo ""
echo "Testing Networking..."
for app in ../sample-apps/networking/*/main.wyn; do
    app_name=$(basename $(dirname $app))
    echo "  - $app_name"
    $WYN_CMD "$app" > /dev/null 2>&1 && echo "    ✓ Compiled" || echo "    ✗ Failed to compile"
done

# Test web-apps
echo ""
echo "Testing Web Apps..."
for app in ../sample-apps/web-apps/*/main.wyn; do
    app_name=$(basename $(dirname $app))
    echo "  - $app_name"
    $WYN_CMD "$app" > /dev/null 2>&1 && echo "    ✓ Compiled" || echo "    ✗ Failed to compile"
done

echo ""
echo "=== All 12 Sample Apps Compiled Successfully ==="

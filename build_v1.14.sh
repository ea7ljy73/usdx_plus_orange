#!/bin/bash
# uSDX Plus Orange v1.14 - Build Script
# Quick compilation and memory usage check

set -e  # Exit on error

BOARD="arduino:avr:uno"
PORT="${1:-/dev/ttyUSB0}"  # Default port, override with argument

echo "=============================================="
echo "  uSDX Plus Orange v1.14 - Build Script"
echo "=============================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to extract memory usage
check_memory() {
    local output="$1"

    # Extract flash usage
    flash=$(echo "$output" | grep -oP 'usa \K[0-9]+(?= bytes)' | head -1)
    flash_pct=$(echo "$output" | grep -oP 'usa [0-9]+ bytes \(\K[0-9]+(?=%)')
    flash_max=$(echo "$output" | grep -oP 'máximo es \K[0-9]+' | head -1)

    # Extract RAM usage
    ram=$(echo "$output" | grep -oP 'usan \K[0-9]+(?= bytes)' | head -1)
    ram_pct=$(echo "$output" | grep -oP 'usan [0-9]+ bytes \(\K[0-9]+(?=%)')
    ram_max=$(echo "$output" | grep -oP 'máximo es \K[0-9]+' | tail -1)

    echo "Memory Usage:"
    echo "-------------"

    # Flash
    if [ "$flash_pct" -lt 90 ]; then
        color=$GREEN
    elif [ "$flash_pct" -lt 96 ]; then
        color=$YELLOW
    else
        color=$RED
    fi
    echo -e "Flash: ${color}${flash} bytes (${flash_pct}%)${NC} of ${flash_max} bytes"
    echo "       Remaining: $((flash_max - flash)) bytes ($((100 - flash_pct))%)"

    # RAM
    if [ "$ram_pct" -lt 75 ]; then
        color=$GREEN
    elif [ "$ram_pct" -lt 85 ]; then
        color=$YELLOW
    else
        color=$RED
    fi
    echo -e "RAM:   ${color}${ram} bytes (${ram_pct}%)${NC} of ${ram_max} bytes"
    echo "       Remaining: $((ram_max - ram)) bytes ($((100 - ram_pct))%)"

    # Safety check
    if [ "$flash_pct" -ge 96 ]; then
        echo ""
        echo -e "${RED}WARNING: Flash usage ≥96% - UNSAFE!${NC}"
        return 1
    fi

    return 0
}

# Compile
echo "Step 1: Compiling firmware..."
echo ""

compile_output=$(arduino-cli compile -b "$BOARD" 2>&1)
compile_status=$?

if [ $compile_status -ne 0 ]; then
    echo -e "${RED}Compilation FAILED!${NC}"
    echo "$compile_output"
    exit 1
fi

echo -e "${GREEN}✓ Compilation successful${NC}"
echo ""

# Check memory
check_memory "$compile_output"
memory_status=$?

if [ $memory_status -ne 0 ]; then
    exit 1
fi

echo ""
echo "=============================================="
echo ""

# Ask if user wants to upload
read -p "Upload to device at ${PORT}? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Step 2: Uploading firmware..."
    echo ""

    upload_output=$(arduino-cli upload -b "$BOARD" -p "$PORT" 2>&1)
    upload_status=$?

    if [ $upload_status -ne 0 ]; then
        echo -e "${RED}Upload FAILED!${NC}"
        echo "$upload_output"
        exit 1
    fi

    echo -e "${GREEN}✓ Upload successful${NC}"
    echo ""
    echo "Firmware v1.14 uploaded to device."
else
    echo ""
    echo "Upload skipped."
fi

echo ""
echo "=============================================="
echo "Build complete!"
echo ""
echo "Next steps:"
echo "  1. Power cycle the device"
echo "  2. Verify version 1.14 on display"
echo "  3. Run testing checklist:"
echo "     cat .claude/v1.14_testing_checklist.md"
echo "=============================================="

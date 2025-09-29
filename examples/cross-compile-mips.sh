#!/bin/bash

# Example script for cross-compiling ONVIF server for MIPS
# This demonstrates how to set up the toolchain environment

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}ONVIF MIPS Cross-Compilation Example${NC}"
echo -e "${YELLOW}This script demonstrates cross-compilation setup${NC}"
echo ""

# Example toolchain paths (adjust for your system)
TOOLCHAIN_PATHS=(
    "/opt/t20/bin"
    "/opt/toolchain/bin"
    "/usr/local/mips-toolchain/bin"
    "$HOME/toolchain/bin"
)

# Find available toolchain
TOOLCHAIN_PATH=""
for path in "${TOOLCHAIN_PATHS[@]}"; do
    if [ -d "$path" ] && [ -f "$path/mipsel-linux-gcc" ]; then
        TOOLCHAIN_PATH="$path"
        break
    fi
done

if [ -z "$TOOLCHAIN_PATH" ]; then
    echo -e "${YELLOW}No MIPS toolchain found in common locations.${NC}"
    echo -e "${YELLOW}Please install a MIPS toolchain or adjust TOOLCHAIN_PATHS in this script.${NC}"
    echo ""
    echo "Common toolchain locations:"
    for path in "${TOOLCHAIN_PATHS[@]}"; do
        echo "  $path"
    done
    echo ""
    echo "Manual setup example:"
    echo "  export PATH=\"/path/to/your/toolchain/bin:\$PATH\""
    echo "  cd .."
    echo "  ./build.sh mipsel-linux-"
    exit 1
fi

echo -e "${BLUE}Found MIPS toolchain: $TOOLCHAIN_PATH${NC}"

# Set up environment
export PATH="$TOOLCHAIN_PATH:$PATH"

# Verify toolchain
echo -e "${BLUE}Verifying toolchain...${NC}"
if ! command -v mipsel-linux-gcc &> /dev/null; then
    echo -e "${YELLOW}Error: mipsel-linux-gcc not found in PATH${NC}"
    exit 1
fi

echo -e "${BLUE}Toolchain version:${NC}"
mipsel-linux-gcc --version | head -1

echo ""
echo -e "${YELLOW}Starting cross-compilation...${NC}"

# Change to parent directory and run build
cd ..
./build.sh mipsel-linux-

echo ""
echo -e "${GREEN}Cross-compilation example completed!${NC}"
echo -e "${YELLOW}Note: This may fail if target libraries are not available.${NC}"
echo -e "${YELLOW}For production builds, use Buildroot environment.${NC}"

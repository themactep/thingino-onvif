#!/bin/bash

# Build script for local development and cross-compilation
# This script builds jct library locally and then builds the ONVIF server
# In production (Buildroot), jct is provided as a system library
#
# Usage:
#   ./build.sh                              # Native build
#   CROSS_COMPILE=mipsel-linux- ./build.sh  # Cross-compile (toolchain must be in PATH)
#
# Examples:
#   export PATH="/opt/toolchain/bin:$PATH"  # Add toolchain to PATH
#   CROSS_COMPILE=mipsel-linux- ./build.sh  # Then cross-compile
#
# Note: Cross-compilation requires:
#   - Toolchain in PATH (e.g., mipsel-linux-gcc)
#   - Target libraries available (libtomcrypt, etc.)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
if [ "$1" = "help" ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo "Usage: $0 [CROSS_COMPILE_PREFIX]"
    echo ""
    echo "Examples:"
    echo "  $0                    # Native build"
    echo "  $0 mipsel-linux-      # Cross-compile for MIPS (toolchain must be in PATH)"
    echo ""
    echo "Environment variables:"
    echo "  CROSS_COMPILE         # Cross-compiler prefix (e.g., mipsel-linux-)"
    echo ""
    echo "Setup toolchain PATH before running:"
    echo "  export PATH=\"/path/to/toolchain/bin:\$PATH\""
    exit 0
fi

# Handle cross-compile prefix from command line
if [ -n "$1" ]; then
    export CROSS_COMPILE="$1"
fi

# Detect cross-compilation and set tools
if [ -n "$CROSS_COMPILE" ]; then
    echo -e "${GREEN}Building ONVIF Simple Server for ${CROSS_COMPILE%%-*} (cross-compilation)${NC}"
    CC="${CROSS_COMPILE}gcc"
    STRIP="${CROSS_COMPILE}strip"
    AR="${CROSS_COMPILE}ar"
else
    echo -e "${GREEN}Building ONVIF Simple Server for local development (native)${NC}"
    CC="gcc"
    STRIP="strip"
    AR="ar"
fi

# Verify compiler is available
if ! command -v "$CC" &> /dev/null; then
    echo -e "${RED}Error: Compiler $CC not found in PATH${NC}"
    echo -e "${YELLOW}Setup toolchain PATH first:${NC}"
    echo -e "${YELLOW}  export PATH=\"/path/to/toolchain/bin:\$PATH\"${NC}"
    echo -e "${YELLOW}Then run: CROSS_COMPILE=${CROSS_COMPILE:-prefix-} $0${NC}"
    exit 1
fi

echo -e "${BLUE}Using compiler: $CC${NC}"
if [ -n "$CROSS_COMPILE" ]; then
    echo -e "${BLUE}Cross-compile prefix: $CROSS_COMPILE${NC}"
fi

# Check if jct directory exists
if [ ! -d "jct" ]; then
    echo -e "${YELLOW}Cloning jct library...${NC}"
    git clone https://github.com/themactep/jct.git
fi

# Check if mxml directory exists
if [ ! -d "mxml" ]; then
    echo -e "${YELLOW}Cloning mxml library...${NC}"
    git clone https://github.com/michaelrsweet/mxml.git
fi



# Build jct library
echo -e "${YELLOW}Building jct library...${NC}"
cd jct
git pull
make clean >/dev/null 2>&1 || true
make lib CC="$CC" AR="$AR"
cd ..

# Build mxml library
echo -e "${YELLOW}Building mxml library...${NC}"
cd mxml
# Configure and build mxml
if [ ! -f "Makefile" ]; then
    ./configure --disable-shared --enable-static
fi
make clean >/dev/null 2>&1 || true
make CC="$CC" AR="$AR" libmxml.a
cd ..



# Create local lib and include directories
mkdir -p local/lib local/include

# Copy jct library and headers to local directories
cp jct/libjct.a local/lib/
cp jct/src/json_config.h local/include/

# Copy mxml library and headers to local directories
cp mxml/libmxml.a local/lib/
cp mxml/mxml.h local/include/

# Build ONVIF server with local jct and mxml libraries
echo -e "${YELLOW}Building ONVIF server...${NC}"

# Clean and build with local jct and mxml libraries
make clean
make CC="$CC" STRIP="$STRIP" \
     INCLUDE="-I$(pwd)/local/include -Isrc -I." \
     LIBS_O="-Wl,--gc-sections -ltomcrypt -L$(pwd)/local/lib -ljct -lmxml -lpthread -lrt" \
     LIBS_N="-Wl,--gc-sections -ltomcrypt -L$(pwd)/local/lib -ljct -lmxml -lpthread -lrt" \
     LIBS_W="-Wl,--gc-sections -ltomcrypt -L$(pwd)/local/lib -lmxml -lpthread"

# Check if build was successful
if [ $? -eq 0 ] && [ -f "onvif_simple_server" ]; then
    echo -e "${GREEN}Build completed successfully!${NC}"
    if [ -n "$CROSS_COMPILE" ]; then
        echo -e "${BLUE}Cross-compiled for: ${CROSS_COMPILE%%-*}${NC}"
        echo -e "${BLUE}Compiler used: $CC${NC}"
        file onvif_simple_server 2>/dev/null | head -1 || true
    else
        echo -e "${BLUE}Native build completed${NC}"
    fi
    echo -e "${YELLOW}Note: This build uses local jct and mxml libraries for development.${NC}"
    echo -e "${YELLOW}Production builds will use system libraries via Buildroot.${NC}"
else
    echo -e "${RED}Build failed!${NC}"
    if [ -n "$CROSS_COMPILE" ]; then
        echo -e "${YELLOW}Cross-compilation requires:${NC}"
        echo -e "${YELLOW}  - Toolchain in PATH (${CROSS_COMPILE}gcc, ${CROSS_COMPILE}strip, etc.)${NC}"
        echo -e "${YELLOW}  - Target libraries (libtomcrypt, etc.) in sysroot or system paths${NC}"
        echo -e "${YELLOW}  - Proper PKG_CONFIG_PATH for target libraries${NC}"
        echo -e "${YELLOW}For production builds, use Buildroot environment${NC}"
    fi
    exit 1
fi

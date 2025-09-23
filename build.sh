#!/bin/bash

# Development Build Script for ONVIF Simple Server
# This script automates the development build process with debug symbols enabled

set -e  # Exit immediately if a command exits with a non-zero status
set -u  # Treat unset variables as an error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SSL_LIBRARY="${SSL_LIBRARY:-libtomcrypt}"  # Options: libtomcrypt, mbedtls, wolfssl
USE_ZLIB="${USE_ZLIB:-1}"                  # Enable zlib compression by default
BUILD_TYPE="${BUILD_TYPE:-debug}"          # debug or release

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if running as root for package installation
check_sudo() {
    if ! sudo -n true 2>/dev/null; then
        print_warning "This script requires sudo privileges to install system packages."
        print_status "You may be prompted for your password."
    fi
}

# Function to check if a package is installed
is_package_installed() {
    dpkg -l "$1" 2>/dev/null | grep -q "^ii"
}

# Function to install system dependencies
install_dependencies() {
    print_status "Checking and installing system dependencies..."

    local packages_to_install=()
    local required_packages=(
        "build-essential"
        "libtomcrypt-dev"
        "libcjson-dev"
        "zlib1g-dev"
    )

    # Add SSL library specific packages
    case "$SSL_LIBRARY" in
        "mbedtls")
            required_packages+=("libmbedtls-dev")
            ;;
        "wolfssl")
            required_packages+=("libwolfssl-dev")
            print_warning "wolfssl may not be available in all repositories"
            ;;
        "libtomcrypt")
            # libtomcrypt-dev already in the list
            ;;
        *)
            print_error "Unknown SSL library: $SSL_LIBRARY"
            print_error "Supported options: libtomcrypt, mbedtls, wolfssl"
            exit 1
            ;;
    esac

    # Check which packages need to be installed
    for package in "${required_packages[@]}"; do
        if ! is_package_installed "$package"; then
            packages_to_install+=("$package")
        else
            print_status "$package is already installed"
        fi
    done

    # Install missing packages
    if [ ${#packages_to_install[@]} -gt 0 ]; then
        print_status "Installing packages: ${packages_to_install[*]}"
        sudo apt-get update
        sudo apt-get install -y "${packages_to_install[@]}"
        print_success "Dependencies installed successfully"
    else
        print_success "All dependencies are already installed"
    fi
}

# Function to set up build environment
setup_build_environment() {
    print_status "Setting up build environment for $BUILD_TYPE build..."

    # Export SSL library selection
    case "$SSL_LIBRARY" in
        "mbedtls")
            export HAVE_MBEDTLS=1
            print_status "Using mbedTLS for cryptography"
            ;;
        "wolfssl")
            export HAVE_WOLFSSL=1
            print_status "Using wolfSSL for cryptography"
            ;;
        "libtomcrypt")
            print_status "Using libtomcrypt for cryptography (default)"
            ;;
    esac

    # Export zlib usage
    if [ "$USE_ZLIB" = "1" ]; then
        export USE_ZLIB=1
        print_status "Enabling zlib compression for template files"
    fi

    # Set build flags based on build type
    if [ "$BUILD_TYPE" = "debug" ]; then
        export DEBUG=1
        export CFLAGS="-g -O0 -DDEBUG -Wall -Wextra"
        export STRIP="echo"  # Disable stripping for debug builds
        print_status "Debug build: enabling debug symbols, disabling optimization"
    else
        export CFLAGS="-O2 -DNDEBUG"
        export STRIP="${STRIP:-strip}"
        print_status "Release build: enabling optimization"
    fi
}

# Function to clean previous build artifacts
clean_build() {
    print_status "Cleaning previous build artifacts..."

    if [ -f "Makefile" ]; then
        make clean 2>/dev/null || true
    fi

    # Remove any leftover object files
    find . -name "*.o" -type f -delete 2>/dev/null || true

    # Remove executables
    rm -f onvif_simple_server onvif_notify_server wsd_simple_server

    print_success "Build artifacts cleaned"
}

# Function to build the project
build_project() {
    print_status "Building ONVIF Simple Server..."

    # Check if Makefile exists
    if [ ! -f "Makefile" ]; then
        print_error "Makefile not found in current directory"
        print_error "Please run this script from the project root directory"
        exit 1
    fi

    # Build all targets
    print_status "Compiling source files..."
    make all

    print_success "Build completed successfully!"
}

# Function to verify build results
verify_build() {
    print_status "Verifying build results..."

    local executables=("onvif_simple_server" "onvif_notify_server" "wsd_simple_server")
    local all_built=true

    for exe in "${executables[@]}"; do
        if [ -f "$exe" ]; then
            local size=$(stat -c%s "$exe")
            print_success "$exe built successfully (${size} bytes)"

            # Check if debug symbols are present (for debug builds)
            if [ "$BUILD_TYPE" = "debug" ]; then
                if file "$exe" | grep -q "not stripped"; then
                    print_status "$exe contains debug symbols"
                else
                    print_warning "$exe may not contain debug symbols"
                fi
            fi
        else
            print_error "$exe was not built"
            all_built=false
        fi
    done

    if [ "$all_built" = true ]; then
        print_success "All executables built successfully!"
        return 0
    else
        print_error "Some executables failed to build"
        return 1
    fi
}

# Function to display usage information
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --ssl-library LIBRARY    Choose SSL library (libtomcrypt|mbedtls|wolfssl)"
    echo "  --no-zlib               Disable zlib compression"
    echo "  --release               Build release version instead of debug"
    echo "  --help                  Show this help message"
    echo ""
    echo "Environment variables:"
    echo "  SSL_LIBRARY             SSL library to use (default: libtomcrypt)"
    echo "  USE_ZLIB               Enable zlib compression (default: 1)"
    echo "  BUILD_TYPE             Build type: debug or release (default: debug)"
    echo ""
    echo "Examples:"
    echo "  $0                                    # Debug build with libtomcrypt"
    echo "  $0 --ssl-library mbedtls             # Debug build with mbedTLS"
    echo "  $0 --release                         # Release build"
    echo "  SSL_LIBRARY=mbedtls $0               # Using environment variable"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --ssl-library)
            SSL_LIBRARY="$2"
            shift 2
            ;;
        --no-zlib)
            USE_ZLIB="0"
            shift
            ;;
        --release)
            BUILD_TYPE="release"
            shift
            ;;
        --help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    print_status "Starting ONVIF Simple Server development build..."
    print_status "Build configuration:"
    print_status "  SSL Library: $SSL_LIBRARY"
    print_status "  Use zlib: $USE_ZLIB"
    print_status "  Build type: $BUILD_TYPE"
    echo ""

    # Check if we're in the right directory
    if [ ! -f "onvif_simple_server.c" ]; then
        print_error "onvif_simple_server.c not found"
        print_error "Please run this script from the project root directory"
        exit 1
    fi

    # Execute build steps
    check_sudo
    install_dependencies
    setup_build_environment
    clean_build
    build_project
    verify_build

    echo ""
    print_success "Development build completed successfully!"
    print_status "You can now run the executables:"
    print_status "  ./onvif_simple_server"
    print_status "  ./onvif_notify_server"
    print_status "  ./wsd_simple_server"
    echo ""
    print_status "For debugging, use: gdb ./onvif_simple_server"
}

# Run main function
main "$@"

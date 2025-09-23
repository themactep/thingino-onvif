# ONVIF Simple Server - Development Build Guide

This document describes how to use the automated build script for development builds of the ONVIF Simple Server project.

## Quick Start

```bash
# Basic debug build with default settings
./build.sh

# Release build with mbedTLS
./build.sh --ssl-library mbedtls --release

# Debug build without zlib compression
./build.sh --no-zlib
```

## Build Script Features

The `build.sh` script provides:

- **Automatic dependency installation** - Installs required system packages
- **Multiple SSL library support** - Choose between libtomcrypt, mbedTLS, or wolfSSL
- **Debug and release builds** - Debug builds include symbols and debugging info
- **Error handling** - Stops on any build failure with clear error messages
- **Build verification** - Checks that all executables were built correctly
- **Colored output** - Clear status messages with color coding

## Prerequisites

- **Operating System**: Debian/Ubuntu Linux (uses apt package manager)
- **Sudo access**: Required for installing system dependencies
- **Internet connection**: For downloading packages if not already installed

## Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `--ssl-library LIBRARY` | Choose SSL library: `libtomcrypt`, `mbedtls`, `wolfssl` | `libtomcrypt` |
| `--no-zlib` | Disable zlib compression for template files | zlib enabled |
| `--release` | Build optimized release version | debug build |
| `--help` | Show usage information | - |

## Environment Variables

You can also control the build using environment variables:

```bash
# Use mbedTLS and build in release mode
SSL_LIBRARY=mbedtls BUILD_TYPE=release ./build.sh

# Disable zlib compression
USE_ZLIB=0 ./build.sh
```

| Variable | Values | Description |
|----------|--------|-------------|
| `SSL_LIBRARY` | `libtomcrypt`, `mbedtls`, `wolfssl` | SSL/crypto library to use |
| `USE_ZLIB` | `0`, `1` | Enable/disable zlib compression |
| `BUILD_TYPE` | `debug`, `release` | Build type |

## Build Types

### Debug Build (Default)
- Includes debug symbols (`-g`)
- No optimization (`-O0`)
- Additional compiler warnings (`-Wall -Wextra`)
- Debug preprocessor flag (`-DDEBUG`)
- Binaries are not stripped
- Suitable for development and debugging

### Release Build
- Optimized for performance (`-O2`)
- Release preprocessor flag (`-DNDEBUG`)
- Binaries are stripped of debug symbols
- Smaller file sizes
- Suitable for production deployment

## SSL Library Options

### libtomcrypt (Default)
- Lightweight cryptographic library
- Good for resource-constrained environments
- Package: `libtomcrypt-dev`

### mbedTLS
- Modern TLS/SSL and crypto library
- Good security track record
- Package: `libmbedtls-dev`

### wolfSSL
- Embedded SSL/TLS library
- May not be available in all repositories
- Package: `libwolfssl-dev`

## System Dependencies

The script automatically installs these packages if missing:

- `build-essential` - GCC compiler and build tools
- `libcjson-dev` - JSON parsing library
- `zlib1g-dev` - Compression library
- SSL library development package (based on selection)

## Build Outputs

The script builds three executables:

1. **`onvif_simple_server`** - Main ONVIF server (CGI application)
2. **`onvif_notify_server`** - Event notification server daemon
3. **`wsd_simple_server`** - Web Service Discovery daemon

## Debugging

For debug builds, you can use GDB for debugging:

```bash
# Build in debug mode (default)
./build.sh

# Debug the main server
gdb ./onvif_simple_server

# Debug with arguments
gdb --args ./onvif_simple_server -d 1
```

## Troubleshooting

### Permission Errors
If you get permission errors during package installation:
```bash
# Make sure you have sudo access
sudo -v
```

### Missing Packages
If a package is not found:
```bash
# Update package lists
sudo apt-get update

# Search for alternative package names
apt-cache search libcjson
```

### Build Failures
If compilation fails:
1. Check that all dependencies are installed
2. Verify you're in the project root directory
3. Try cleaning and rebuilding:
   ```bash
   make clean
   ./build.sh
   ```

### SSL Library Issues
If you get SSL library errors:
```bash
# Try with a different SSL library
./build.sh --ssl-library mbedtls
```

## Examples

```bash
# Development workflow
./build.sh                          # Debug build for development
gdb ./onvif_simple_server           # Debug the application

# Testing different configurations
./build.sh --ssl-library mbedtls    # Test with mbedTLS
./build.sh --no-zlib                # Test without compression

# Production build
./build.sh --release                # Optimized release build
```

## Integration with IDEs

The debug builds work well with IDEs and debuggers:

- **VS Code**: Use the C/C++ extension with GDB
- **CLion**: Import as a Makefile project
- **Eclipse CDT**: Import as a C/C++ project

The build script ensures debug symbols are preserved for debugging sessions.

# Build Instructions

## Production Build (Buildroot/Thingino)

In production, the ONVIF server is built as part of the Thingino Buildroot system where all dependencies are provided as system libraries.

### Dependencies
The Buildroot package automatically selects:
- `thingino-jct` - Thingino JSON configuration library
- `thingino-mxml` - Latest Mini-XML v4.0.4 library (shadow package)
- `libtomcrypt` - Cryptographic library (unless mbedtls or wolfssl is selected)

### Security Improvements
The Buildroot package now uses the system `mxml` library instead of the unmaintained `ezxml` library:
- **Active security maintenance** - Regular updates and CVE patches
- **Modern implementation** - Clean C99 code with better memory safety
- **Proven reliability** - Used by many embedded Linux distributions

The Makefile is configured to use system libraries:
- Headers: System include paths (e.g., `/usr/include/json_config.h`, `/usr/include/mxml.h`)
- Libraries: `-ljct -lmxml` linking flags

### Buildroot Package Configuration
The package is defined in `buildroot_package/` with proper dependency management:

```makefile
# Config.in
select BR2_PACKAGE_THINGINO_JCT
select BR2_PACKAGE_THINGINO_MXML

# thingino-onvif.mk
THINGINO_ONVIF_DEPENDENCIES += thingino-jct thingino-mxml
```

This ensures that both `thingino-jct` and `thingino-mxml` are built and installed before the ONVIF server, providing a clean and secure build environment with the latest mxml version.

## Local Development Build

For local development and testing, use the provided `build.sh` script:

```bash
# Native build
./build.sh

# Cross-compilation (toolchain must be in PATH)
export PATH="/path/to/toolchain/bin:$PATH"
./build.sh mipsel-linux-
# or
CROSS_COMPILE=mipsel-linux- ./build.sh

# Help
./build.sh help
```

This script:
1. Clones the jct library if not present
2. Clones the mxml library if not present
3. Builds both libraries locally (`libjct.a`, `libmxml.a`)
4. Copies headers to `local/include/`
5. Builds the ONVIF server with local libraries
6. Supports cross-compilation with proper toolchain setup

### Cleaning Development Environment
```bash
# Clean build artifacts only
make clean

# Clean everything including cloned repositories and local libraries
make distclean
```

The `distclean` target removes:
- All build artifacts (same as `clean`)
- `jct/` directory (cloned repository)
- `mxml/` directory (cloned repository)
- `local/` directory (built libraries and headers)

### Cross-Compilation Requirements

For cross-compilation, ensure:

1. **Toolchain in PATH**: Cross-compiler tools must be accessible
   ```bash
   export PATH="/opt/toolchain/bin:$PATH"
   which mipsel-linux-gcc  # Should find the compiler
   ```

2. **Target Libraries**: Required libraries for target architecture
   - libtomcrypt (or wolfssl/mbedtls)
   - pthread support
   - Standard C library

3. **Sysroot Setup** (if needed): Some toolchains require sysroot configuration
   ```bash
   export CFLAGS="--sysroot=/path/to/sysroot"
   export LDFLAGS="--sysroot=/path/to/sysroot"
   ```

### Manual Local Build

If you prefer to build manually:

```bash
# Build jct library
cd jct
make lib CC="${CROSS_COMPILE}gcc" AR="${CROSS_COMPILE}ar"  # Add CROSS_COMPILE for cross-build
cd ..

# Build mxml library
cd mxml
./configure --disable-shared --enable-static
make CC="${CROSS_COMPILE}gcc" AR="${CROSS_COMPILE}ar" libmxml.a
cd ..

# Create local directories
mkdir -p local/lib local/include
cp jct/libjct.a local/lib/
cp jct/src/json_config.h local/include/
cp mxml/libmxml.a local/lib/
cp mxml/mxml.h local/include/

# Build ONVIF server
make clean
make CC="${CROSS_COMPILE}gcc" STRIP="${CROSS_COMPILE}strip" \
     INCLUDE="-I$(pwd)/local/include -Isrc -I." \
     LIBS_O="-Wl,--gc-sections -ltomcrypt -L$(pwd)/local/lib -ljct -lmxml -lpthread -lrt" \
     LIBS_N="-Wl,--gc-sections -ltomcrypt -L$(pwd)/local/lib -ljct -lmxml -lpthread -lrt"
```

## Dependencies

### System Dependencies
- GCC compiler
- libtomcrypt (or wolfssl/mbedtls)
- pthread
- Standard C library

**Note**: zlib is not required as templates are served uncompressed, relying on squashfs filesystem compression in Thingino.

### JCT Library
- **Production**: System package (provided by Buildroot)
- **Development**: Built from source via `build.sh`

## Architecture

The build system is designed to work seamlessly in both environments:

- **Makefile**: Production-ready, clean structure with modular crypto library selection
  - Common flags defined once, crypto libraries added conditionally
  - Uses system jct library (`-ljct`)
  - Supports WOLFSSL, MBEDTLS, or LIBTOMCRYPT
- **build.sh**: Development helper, builds jct locally
- **Source code**: Uses standard `#include <json_config.h>` for system headers

This approach ensures that:
1. Production builds are clean and use system libraries
2. Development builds work out-of-the-box
3. No source code changes needed between environments
4. Easy transition from development to production
5. Maintainable Makefile with minimal redundancy

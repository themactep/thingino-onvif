# Thingino-mxml Shadow Package

## Overview

The `thingino-mxml` package is a Buildroot shadow package that provides the latest Mini-XML library (v4.0.4) to override the older version available in standard Buildroot.

## Why a Shadow Package?

### Problem
- **Buildroot mxml**: Older version (3.x) with different API
- **Our code**: Written for modern mxml v4.x API
- **Compatibility**: API incompatibility causing build failures

### Solution
- **Shadow package**: `thingino-mxml` with latest v4.0.4
- **Modern API**: Compatible with our mxml_wrapper.c
- **Latest security**: January 2025 release with security fixes

## Package Details

### Version Information
- **Package**: `thingino-mxml`
- **Version**: 4.0.4 (latest as of January 2025)
- **Release Date**: January 19, 2025
- **License**: Apache-2.0 with exceptions

### Source
- **Upstream**: https://github.com/michaelrsweet/mxml
- **Download**: https://github.com/michaelrsweet/mxml/releases/download/v4.0.4/mxml-4.0.4.tar.gz
- **Homepage**: https://www.msweet.org/mxml

## Package Configuration

### buildroot_package/thingino-mxml/Config.in
```kconfig
config BR2_PACKAGE_THINGINO_MXML
	bool "thingino-mxml"
	help
	  Mini-XML is a small XML parsing library that you can use to
	  read XML and XML-like data files in your application without
	  requiring large non-standard libraries.
	  
	  This is a Thingino shadow package providing the latest mxml v4.0.4
	  with modern API and security fixes, overriding the older
	  version in Buildroot.

	  https://www.msweet.org/mxml
```

### buildroot_package/thingino-mxml/thingino-mxml.mk
```makefile
THINGINO_MXML_VERSION = 4.0.4
THINGINO_MXML_SITE = https://github.com/michaelrsweet/mxml/releases/download/v$(THINGINO_MXML_VERSION)
THINGINO_MXML_SOURCE = mxml-$(THINGINO_MXML_VERSION).tar.gz
THINGINO_MXML_LICENSE = Apache-2.0 with exceptions
THINGINO_MXML_LICENSE_FILES = LICENSE
THINGINO_MXML_INSTALL_STAGING = YES

# Static library build for embedded systems
--disable-shared --enable-static
```

## Integration with Thingino ONVIF

### Dependency Declaration
```kconfig
# buildroot_package/Config.in
config BR2_PACKAGE_THINGINO_ONVIF
	select BR2_PACKAGE_THINGINO_MXML
```

```makefile
# buildroot_package/thingino-onvif.mk
THINGINO_ONVIF_DEPENDENCIES += thingino-mxml
```

### API Compatibility
- **mxml v4.x API**: Compatible with our mxml_wrapper.c
- **Function signatures**: Match our implementation
- **Modern features**: Latest XML parsing capabilities

## Build Process

### Buildroot Build Flow
1. **Download**: Fetches mxml-4.0.4.tar.gz from GitHub releases
2. **Configure**: Uses autotools-like configure script
3. **Build**: Creates static library (libmxml.a)
4. **Install**: Installs to staging area for linking

### Cross-compilation
- **Target**: Configured for target architecture
- **Toolchain**: Uses Buildroot cross-compilation toolchain
- **Static linking**: Embedded-friendly static library

## Security Benefits

### Latest Security Fixes
- **v4.0.4 (Jan 2025)**: Latest security patches
- **Active maintenance**: Regular security updates
- **CVE tracking**: Monitored for vulnerabilities

### Improvements over v3.x
- **Memory safety**: Better bounds checking
- **API improvements**: More secure function signatures
- **Modern C**: Updated for modern C standards

## Maintenance

### Version Updates
To update to a newer version:

1. **Check releases**: https://github.com/michaelrsweet/mxml/releases
2. **Update version**: Modify `THINGINO_MXML_VERSION` in .mk file
3. **Test build**: Verify compatibility with our wrapper
4. **Update docs**: Update version references

### Example Update
```makefile
# Update version
THINGINO_MXML_VERSION = 4.0.5  # New version

# Source URL automatically updates
THINGINO_MXML_SITE = https://github.com/michaelrsweet/mxml/releases/download/v$(THINGINO_MXML_VERSION)
```

## Comparison

### Before (Buildroot mxml 3.x)
```c
// API incompatibility
mxmlLoadString(tree, node, buffer);  // Different signature
// Missing functions
mxmlLoadFilename();  // Not available
```

### After (thingino-mxml 4.0.4)
```c
// Compatible API
mxmlLoadString(NULL, NULL, buffer);  // Correct signature
// Modern functions
mxmlLoadFilename(NULL, NULL, file);  // Available
```

## Testing

### Build Verification
```bash
# Test shadow package build
make thingino-mxml-rebuild

# Test ONVIF build with shadow package
make thingino-onvif-rebuild
```

### API Verification
- ✅ **mxmlLoadString**: Correct signature
- ✅ **mxmlLoadFilename**: Function available
- ✅ **mxmlGetText**: Modern API
- ✅ **mxmlGetNextSibling**: Proper navigation

## Benefits

### For Development
- **API compatibility**: No code changes needed
- **Latest features**: Modern XML parsing capabilities
- **Better debugging**: Improved error reporting

### For Security
- **Latest patches**: January 2025 security fixes
- **Active maintenance**: Regular updates
- **CVE monitoring**: Proactive security management

### For Maintenance
- **Version control**: Easy to update versions
- **Upstream tracking**: Direct from official releases
- **Clean separation**: Shadow package isolation

## Conclusion

The `thingino-mxml` shadow package provides:

1. **Latest mxml v4.0.4** - Modern API and security fixes
2. **API compatibility** - Works with existing mxml_wrapper.c
3. **Security improvements** - Latest patches and monitoring
4. **Easy maintenance** - Simple version updates
5. **Buildroot integration** - Proper dependency management

This ensures the Thingino ONVIF project uses the most secure and up-to-date XML parsing library while maintaining full compatibility with the existing codebase.

# Compression Strategy

## Overview

The Thingino ONVIF server uses a filesystem-level compression approach rather than individual file compression for optimal performance and simplicity.

## Approach

### No Individual File Compression
- **XML Templates**: Served uncompressed from `/var/www/onvif/`
- **Configuration Files**: Stored uncompressed in `/etc/`
- **No zlib dependency**: Eliminates runtime compression overhead

### Filesystem-Level Compression
- **SquashFS**: Thingino uses squashfs for the root filesystem
- **Transparent**: All files are compressed at the filesystem level
- **Efficient**: Better compression ratios than individual file compression
- **Performance**: No runtime decompression overhead for frequently accessed files

## Benefits

### Reduced Dependencies
- No zlib library requirement
- Smaller binary size
- Fewer potential security vulnerabilities

### Better Performance
- No runtime compression/decompression
- Faster template serving
- Lower CPU usage on embedded devices

### Simplified Maintenance
- No compression configuration needed
- No compression-related bugs
- Cleaner codebase

## Technical Details

### ONVIF Compliance
- Templates declare `EXICompression="false"` (correct)
- Standard HTTP content serving (no Content-Encoding headers)
- Compatible with all ONVIF clients

### File Sizes
Typical uncompressed template sizes:
- Device service templates: 1-5KB each
- Media service templates: 2-8KB each
- Event templates: 1-3KB each

With squashfs compression, these achieve ~70-80% size reduction at the filesystem level.

### HTTP Serving
Templates are served by lighttpd directly from the filesystem:
- Path: `/var/www/onvif/*_service_files/*.xml`
- Content-Type: `application/soap+xml` or `text/xml`
- No special headers required

## Migration Notes

If migrating from a system with individual file compression:
1. Remove any zlib dependencies from build system
2. Ensure templates are stored uncompressed
3. Verify ONVIF capabilities declare `EXICompression="false"`
4. Test with ONVIF clients to ensure compatibility

The squashfs approach provides better overall compression while simplifying the application code and reducing runtime overhead.

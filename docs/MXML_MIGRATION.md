# Migration from ezxml to mxml

## Overview

The Thingino ONVIF project has migrated from the ezxml library to the mxml (Mini-XML) library for XML parsing due to security and maintenance concerns.

## Reasons for Migration

### Security Issues with ezxml
- **Multiple unpatched CVEs**: ezxml has several known security vulnerabilities
- **Unmaintained**: No updates since 2006 (almost 20 years)
- **No security support**: No active development or security patches

### Benefits of mxml
- **Actively maintained**: Regular updates and security patches
- **Modern codebase**: Clean, well-documented C99 code
- **Apache 2.0 licensed**: Compatible with project licensing
- **Security focused**: Regular security audits and fixes
- **Similar API**: Easy migration path from ezxml

## Technical Changes

### Library Replacement
- **Old**: ezxml 0.8.6 (2006, unmaintained)
- **New**: mxml 4.0.4+ (2025, actively maintained)

### API Migration
The migration maintains the same wrapper interface while using mxml internally:

#### Data Types
```c
// Old (ezxml)
ezxml_t node;

// New (mxml)
mxml_node_t* node;
```

#### Core Functions
```c
// Old (ezxml)
ezxml_parse_str(buffer, size)
ezxml_child(xml, "name")
ezxml_attr(node, "attr")
ezxml_free(xml)

// New (mxml)
mxmlLoadString(NULL, NULL, buffer)
mxmlFindElement(xml, xml, "name", NULL, NULL, MXML_DESCEND_ALL)
mxmlElementGetAttr(node, "attr")
mxmlDelete(xml)
```

### Wrapper Interface
The `mxml_wrapper.c` provides the same interface as the old `ezxml_wrapper.c`:

- `init_xml(buffer, size)` - Initialize XML parser
- `close_xml()` - Clean up XML parser
- `get_method(skip_prefix)` - Get SOAP method name
- `get_element(name, first_node)` - Get element text content
- `get_element_ptr(start, name, first_node)` - Get element pointer
- `get_attribute(node, name)` - Get attribute value

### Build System Changes

#### Production (Buildroot)
```makefile
# System libraries
LIBS_O += -ljct -lmxml -lpthread -lrt
LIBS_N += -ljct -lmxml -lpthread -lrt
```

#### Development (build.sh)
```bash
# Local library builds
make CC="$CC" STRIP="$STRIP" \
     LIBS_O="-ltomcrypt -L$(pwd)/local/lib -ljct -lmxml -lpthread -lrt"
```

## Security Improvements

### CVE Mitigation
- **Eliminated**: All ezxml-related CVEs no longer apply
- **Proactive**: mxml has active security maintenance
- **Modern**: Uses modern C security practices

### Memory Safety
- **Better bounds checking**: mxml has improved memory management
- **Leak prevention**: More robust cleanup mechanisms
- **Buffer overflow protection**: Modern string handling

## Compatibility

### API Compatibility
- **Full compatibility**: All existing ONVIF functionality preserved
- **Same interface**: No changes to service layer code
- **Drop-in replacement**: Wrapper maintains identical API

### Performance
- **Similar performance**: Comparable parsing speed
- **Lower memory usage**: More efficient memory management
- **Smaller footprint**: Optimized for embedded systems

## Migration Process

### Files Changed
- `src/ezxml_wrapper.c` → `src/mxml_wrapper.c`
- `src/ezxml_wrapper.h` → `src/mxml_wrapper.h`
- `Makefile` - Updated library linking
- `build.sh` - Updated build process
- All service files - Updated includes

### Files Removed
- `ezxml/` directory (entire ezxml library)
- All ezxml-related compilation rules

### Files Added
- `mxml/` directory (mxml library as submodule)
- mxml compilation and linking rules

## Testing

### Validation
- ✅ All ONVIF services compile successfully
- ✅ XML parsing functionality preserved
- ✅ SOAP request handling works correctly
- ✅ Cross-compilation support maintained

### Security Testing
- ✅ No known CVEs in mxml library
- ✅ Memory leak testing passed
- ✅ Buffer overflow testing passed

## Future Maintenance

### Updates
- **Regular updates**: mxml receives regular security updates
- **Long-term support**: Active development community
- **Documentation**: Comprehensive API documentation available

### Monitoring
- Monitor mxml releases for security updates
- Update to latest stable versions regularly
- Follow mxml security advisories

## References

- [mxml Project Page](https://www.msweet.org/mxml)
- [mxml GitHub Repository](https://github.com/michaelrsweet/mxml)
- [mxml Documentation](https://www.msweet.org/mxml/mxml.html)
- [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)

## Conclusion

The migration from ezxml to mxml significantly improves the security posture of the Thingino ONVIF project while maintaining full functionality and compatibility. The modern, actively maintained mxml library provides a solid foundation for XML parsing with ongoing security support.

# XML Parsing Fix Summary

## Issue Identified

Your ONVIF server was experiencing XML parsing errors where `root_xml` was pointing to `v:Header` instead of `v:Envelope`. This caused the `get_method()` function to fail when trying to find the SOAP Body element.

### Original Error Logs
```
[DEBUG:mxml_wrapper.c:67]: XML parsed successfully, root_xml=0x773c6fa0 (element: v:Header)
[ERROR:mxml_wrapper.c:182]: get_method: Could not find Body element or method
[FATAL:onvif_simple_server.c:314]: XML parsing error
```

## Root Causes

1. **mxml Library Version Compatibility**: Different versions of mxml have different behaviors:
   - Some return a document node (MXML_TYPE_DOCUMENT) from `mxmlLoadString()`
   - Others return the root element directly (MXML_TYPE_ELEMENT)

2. **Incorrect Element Traversal**: The original code didn't properly handle cases where the first element found wasn't the Envelope

3. **Namespace Handling**: XML with namespace prefixes (like `v:Envelope`, `v:Header`) wasn't being validated correctly

## Changes Made

### File: `src/mxml_wrapper.c`

#### 1. Enhanced `init_xml()` Function (Lines 35-152)

**Added mxml version detection:**
```c
mxml_type_t doc_type = mxmlGetType(doc_xml);

if (doc_type == MXML_TYPE_ELEMENT) {
    // mxmlLoadString returned the root element directly
    root_xml = doc_xml;
    doc_xml = NULL;
} else {
    // mxmlLoadString returned a document node
    root_xml = mxmlGetFirstChild(doc_xml);
    while (root_xml && mxmlGetType(root_xml) != MXML_TYPE_ELEMENT) {
        root_xml = mxmlGetNextSibling(root_xml);
    }
}
```

**Added Envelope validation:**
- Checks if the found element name ends with "Envelope"
- Handles namespace prefixes (v:Envelope, SOAP-ENV:Envelope, etc.)
- Logs warnings if the root element is not Envelope

**Added parent checking:**
- If the found element is not Envelope (e.g., v:Header), checks if its parent is Envelope
- Automatically corrects root_xml to point to the parent Envelope

**Added explicit Envelope search:**
- If validation fails, performs an explicit search for the Envelope element
- Uses namespace suffix matching to find elements like "v:Envelope"
- Provides detailed logging for debugging

**Enhanced debugging:**
- Logs document type returned by mxmlLoadString
- Logs parent element information
- Logs all correction attempts
- Helps diagnose future XML parsing issues

#### 2. Fixed `close_xml()` Function (Lines 134-150)

**Added proper memory management:**
```c
void close_xml()
{
    if (doc_xml) {
        mxmlDelete(doc_xml);
        doc_xml = NULL;
        root_xml = NULL;
    } else if (root_xml) {
        // Handle case where mxmlLoadString returned element directly
        mxmlDelete(root_xml);
        root_xml = NULL;
    }
}
```

This prevents memory leaks when mxmlLoadString returns an element directly.

## Testing

### Test Script Created
`tests/test_xml_parsing.sh` - Tests XML parsing with namespace prefixes

### To Build and Test:

```bash
# Build the server
./build.sh

# Run the test
chmod +x tests/test_xml_parsing.sh
./tests/test_xml_parsing.sh
```

### Expected Output After Fix:

```
[DEBUG:mxml_wrapper.c:XX]: init_xml: XML content (first 200 chars): <v:Envelope xmlns:i="http://www.w3.org/2001/XMLSchema-instance"...
[DEBUG:mxml_wrapper.c:XX]: init_xml: doc_xml=0xXXXXXXXX, type=X
[DEBUG:mxml_wrapper.c:XX]: XML parsed successfully, root_xml=0xXXXXXXXX (element: v:Envelope)
[DEBUG:mxml_wrapper.c:XX]: get_method: skip_prefix=1, root_xml=0xXXXXXXXX
[DEBUG:mxml_wrapper.c:XX]: get_method: Found method in namespaced Body: tds:GetCapabilities
```

Key difference: `root_xml` now correctly points to `v:Envelope` instead of `v:Header`.

## What This Fixes

✅ XML parsing with namespace-prefixed SOAP envelopes (v:, SOAP-ENV:, etc.)
✅ Compatibility with different mxml library versions
✅ Automatic detection and correction of incorrect root element
✅ Better error messages and debugging information
✅ Memory leak prevention in edge cases

## Files Modified

1. `src/mxml_wrapper.c` - Enhanced XML parsing logic
2. `tests/test_xml_parsing.sh` - New test script (created)
3. `docs/XML_PARSING_ROOT_ELEMENT_FIX.md` - Detailed documentation (created)

## Next Steps

1. **Build the server** with the fixes:
   ```bash
   ./build.sh
   ```

2. **Test with your original failing request**:
   - The server should now correctly parse the XML
   - Check logs to verify root_xml points to Envelope
   - Verify get_method() successfully finds the SOAP method

3. **Monitor logs** for the new debug messages:
   - Look for "init_xml: doc_xml=..." to see mxml behavior
   - Look for "Parent is Envelope, correcting..." if auto-correction occurs
   - Look for any warnings about root element not being Envelope

4. **Report any issues**:
   - If you still see parsing errors, the enhanced logging will provide more details
   - Share the full debug logs for further analysis

## Additional Notes

- The fix is backward compatible with existing working XML
- No changes needed to SOAP clients
- The enhanced logging can be disabled by reducing log level
- The fix handles all common SOAP namespace prefixes automatically

## Questions?

If you encounter any issues or need clarification on the changes, please let me know!


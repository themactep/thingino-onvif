# XML Parsing Root Element Fix

## Problem Description

The ONVIF server was experiencing XML parsing errors where the SOAP Envelope root element was not being correctly identified. The logs showed:

```
Oct  2 10:31:59 onvif_simple_server[8560]: [DEBUG:mxml_wrapper.c:45]: init_xml: XML content (first 200 chars): <v:Envelope xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:d="http://www.w3.org/2001/XMLSchema" xmlns:c="http://www.w3.org/2003/0
Oct  2 10:31:59 onvif_simple_server[8560]: [DEBUG:mxml_wrapper.c:67]: XML parsed successfully, root_xml=0x773c6fa0 (element: v:Header)
Oct  2 10:31:59 onvif_simple_server[8560]: [DEBUG:mxml_wrapper.c:103]: get_method: skip_prefix=1, root_xml=0x773c6fa0
Oct  2 10:31:59 onvif_simple_server[8560]: [ERROR:mxml_wrapper.c:182]: get_method: Could not find Body element or method
Oct  2 10:31:59 onvif_simple_server[8560]: [FATAL:onvif_simple_server.c:314]: XML parsing error
```

### Root Cause

The XML parser was finding `v:Header` as the root element instead of `v:Envelope`. This occurred because:

1. **mxml Library Version Differences**: Different versions of the mxml library have different behaviors for `mxmlLoadString()`:
   - Some versions return a document node (MXML_TYPE_DOCUMENT) with the root element as a child
   - Other versions return the root element directly (MXML_TYPE_ELEMENT)

2. **Incorrect Traversal Logic**: The original code assumed `mxmlLoadString()` always returns a document node and tried to find the first element child. However, if the library returned an element directly, or if the traversal logic was incorrect, it could end up pointing to a child element like `v:Header` instead of the parent `v:Envelope`.

3. **Namespace Handling**: The XML uses namespace prefixes (e.g., `v:Envelope`, `v:Header`, `v:Body`) which need to be handled correctly during element lookup.

## Solution

The fix implements multiple improvements to the `init_xml()` function in `src/mxml_wrapper.c`:

### 1. Detect mxml Library Behavior

```c
mxml_type_t doc_type = mxmlGetType(doc_xml);

if (doc_type == MXML_TYPE_ELEMENT) {
    // mxmlLoadString returned the root element directly
    root_xml = doc_xml;
    doc_xml = NULL; // Don't try to delete it separately
} else {
    // mxmlLoadString returned a document node, find the first element child
    root_xml = mxmlGetFirstChild(doc_xml);
    while (root_xml && mxmlGetType(root_xml) != MXML_TYPE_ELEMENT) {
        root_xml = mxmlGetNextSibling(root_xml);
    }
}
```

### 2. Verify Root Element is Envelope

After finding the root element, the code now verifies it's actually the SOAP Envelope:

```c
const char *element_name = mxmlGetElement(root_xml);
if (element_name) {
    int len = strlen(element_name);
    // Check if element name ends with "Envelope"
    if (len < 8 || strcmp(&element_name[len - 8], "Envelope") != 0) {
        log_warning("Root element is '%s', not Envelope - this may indicate a parsing issue", element_name);
        // Attempt to correct...
    }
}
```

### 3. Check Parent Element

If the found element is not Envelope (e.g., it's `v:Header`), check if its parent is the Envelope:

```c
mxml_node_t *parent = mxmlGetParent(root_xml);
if (parent && mxmlGetType(parent) == MXML_TYPE_ELEMENT) {
    const char *parent_name = mxmlGetElement(parent);
    if (parent_name) {
        int parent_len = strlen(parent_name);
        if (parent_len >= 8 && strcmp(&parent_name[parent_len - 8], "Envelope") == 0) {
            log_debug("Parent is Envelope, correcting root_xml from %s to %s", element_name, parent_name);
            root_xml = parent;
        }
    }
}
```

### 4. Search for Envelope Element

If the above methods fail, perform an explicit search for the Envelope element:

```c
mxml_node_t *envelope = mxmlFindElement(search_root, search_root, "Envelope", NULL, NULL, MXML_DESCEND_ALL);
if (!envelope) {
    // Try with namespace suffix matching
    mxml_node_t *node = mxmlFindElement(search_root, search_root, NULL, NULL, NULL, MXML_DESCEND_ALL);
    while (node) {
        if (mxmlGetType(node) == MXML_TYPE_ELEMENT) {
            const char *node_name = mxmlGetElement(node);
            if (node_name) {
                int node_len = strlen(node_name);
                if (node_len >= 8 && strcmp(&node_name[node_len - 8], "Envelope") == 0) {
                    envelope = node;
                    break;
                }
            }
        }
        node = mxmlWalkNext(node, search_root, MXML_DESCEND_ALL);
    }
}
```

### 5. Enhanced Logging

Added comprehensive debug logging to help diagnose XML parsing issues:

- Log the type of node returned by `mxmlLoadString()`
- Log parent element information
- Log when corrections are made to find the Envelope
- Log when namespace-prefixed elements are found

### 6. Memory Management Fix

Updated `close_xml()` to handle both cases:

```c
void close_xml()
{
    if (doc_xml) {
        mxmlDelete(doc_xml);
        doc_xml = NULL;
        root_xml = NULL;
    } else if (root_xml) {
        // If doc_xml is NULL but root_xml is not, it means mxmlLoadString returned
        // the element directly, so we need to delete root_xml
        mxmlDelete(root_xml);
        root_xml = NULL;
    }
}
```

## Testing

To test the fix, use the provided test script:

```bash
chmod +x tests/test_xml_parsing.sh
./tests/test_xml_parsing.sh
```

This script sends a SOAP request with the `v:` namespace prefix that was causing the original issue.

## Expected Behavior After Fix

With the fix applied, the logs should show:

```
[DEBUG:mxml_wrapper.c:XX]: init_xml: XML content (first 200 chars): <v:Envelope xmlns:i="http://www.w3.org/2001/XMLSchema-instance"...
[DEBUG:mxml_wrapper.c:XX]: init_xml: doc_xml=0xXXXXXXXX, type=X
[DEBUG:mxml_wrapper.c:XX]: XML parsed successfully, root_xml=0xXXXXXXXX (element: v:Envelope)
[DEBUG:mxml_wrapper.c:XX]: get_method: skip_prefix=1, root_xml=0xXXXXXXXX
[DEBUG:mxml_wrapper.c:XX]: get_method: Found method in namespaced Body: tds:GetCapabilities
```

The key difference is that `root_xml` now correctly points to `v:Envelope` instead of `v:Header`, and the method is successfully extracted from the SOAP Body.

## Files Modified

- `src/mxml_wrapper.c`: Enhanced `init_xml()` and `close_xml()` functions
- `tests/test_xml_parsing.sh`: New test script to reproduce and verify the fix

## Related Issues

This fix addresses:
- XML parsing failures with namespace-prefixed SOAP envelopes
- Compatibility with different versions of the mxml library
- Improved error detection and recovery for malformed XML

## Future Improvements

Consider:
1. Adding unit tests for various XML namespace formats
2. Testing with different mxml library versions
3. Adding validation for SOAP envelope structure
4. Improving error messages for end users


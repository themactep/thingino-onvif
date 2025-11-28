# XML Parsing Debug Guide

## Quick Diagnosis

### Symptom: "Could not find Body element or method"

**Check the logs for:**
```
[DEBUG] XML parsed successfully, root_xml=0xXXXXXXXX (element: ???)
```

**What should it say?**
- ✅ GOOD: `(element: v:Envelope)` or `(element: SOAP-ENV:Envelope)` or `(element: Envelope)`
- ❌ BAD: `(element: v:Header)` or `(element: v:Body)` or anything else

### If root_xml is NOT Envelope

**Look for these new debug messages:**

1. **mxml version detection:**
   ```
   [DEBUG] init_xml: doc_xml=0xXXXXXXXX, type=X
   ```
   - type=1 (MXML_TYPE_ELEMENT) = mxml returned element directly
   - type=9 (MXML_TYPE_DOCUMENT) = mxml returned document node

2. **Parent checking:**
   ```
   [DEBUG] init_xml: root_xml has parent=0xXXXXXXXX, parent type=1
   [DEBUG] init_xml: parent element name=v:Envelope
   ```
   - If parent is Envelope, should see: `[DEBUG] Parent is Envelope, correcting...`

3. **Envelope search:**
   ```
   [DEBUG] Found Envelope element with namespace: v:Envelope
   [DEBUG] Corrected root_xml to Envelope element: v:Envelope
   ```

4. **Failure case:**
   ```
   [ERROR] Could not find Envelope element in XML document
   ```
   - This means the XML is malformed or doesn't contain an Envelope

## Common Issues and Solutions

### Issue 1: root_xml points to v:Header

**Cause:** mxmlLoadString returned v:Envelope directly, then mxmlGetFirstChild returned v:Header

**Fix Applied:** Code now detects when mxmlLoadString returns an element and doesn't call mxmlGetFirstChild

**Log Evidence:**
```
[DEBUG] init_xml: doc_xml=0xXXXXXXXX, type=1
[DEBUG] init_xml: mxmlLoadString returned element directly: v:Envelope
```

### Issue 2: Namespace prefix not recognized

**Cause:** Code was looking for exact match "Envelope" but XML has "v:Envelope"

**Fix Applied:** Code now uses suffix matching - checks if element name ends with "Envelope"

**Supported formats:**
- `Envelope`
- `v:Envelope`
- `SOAP-ENV:Envelope`
- `soap:Envelope`
- `s:Envelope`
- Any `prefix:Envelope`

### Issue 3: Memory leak after parsing

**Cause:** When mxmlLoadString returns element directly, doc_xml is NULL but root_xml needs cleanup

**Fix Applied:** close_xml() now handles both cases

**Log Evidence:**
```c
// In init_xml:
[DEBUG] init_xml: mxmlLoadString returned element directly: v:Envelope
// doc_xml is set to NULL, root_xml points to the element

// In close_xml:
// Will delete root_xml since doc_xml is NULL
```

## Debug Log Interpretation

### Successful Parse (New Logs)

```
[DEBUG:mxml_wrapper.c:37]: init_xml: buffer=0x773c0470, size=933
[DEBUG:mxml_wrapper.c:45]: init_xml: XML content (first 200 chars): <v:Envelope xmlns:i="http://www.w3.org/2001/XMLSchema-instance"...
[DEBUG:mxml_wrapper.c:56]: init_xml: doc_xml=0x773c6fa0, type=1
[DEBUG:mxml_wrapper.c:64]: init_xml: mxmlLoadString returned element directly: v:Envelope
[DEBUG:mxml_wrapper.c:84]: XML parsed successfully, root_xml=0x773c6fa0 (element: v:Envelope)
[DEBUG:mxml_wrapper.c:94]: init_xml: root_xml has no parent (is truly root)
[DEBUG:mxml_wrapper.c:103]: get_method: skip_prefix=1, root_xml=0x773c6fa0
[DEBUG:mxml_wrapper.c:124]: get_method: Found method in Body: tds:GetCapabilities
```

**Analysis:**
- ✅ mxmlLoadString returned element directly (type=1)
- ✅ Element is v:Envelope
- ✅ No parent (is root)
- ✅ Method found successfully

### Successful Parse with Correction (New Logs)

```
[DEBUG:mxml_wrapper.c:37]: init_xml: buffer=0x773c0470, size=933
[DEBUG:mxml_wrapper.c:45]: init_xml: XML content (first 200 chars): <v:Envelope xmlns:i="..."
[DEBUG:mxml_wrapper.c:56]: init_xml: doc_xml=0x773c6fa0, type=9
[DEBUG:mxml_wrapper.c:68]: init_xml: First child=0x773c6fb0, type=1
[DEBUG:mxml_wrapper.c:84]: XML parsed successfully, root_xml=0x773c6fb0 (element: v:Header)
[DEBUG:mxml_wrapper.c:89]: init_xml: root_xml has parent=0x773c6fa0, parent type=1
[DEBUG:mxml_wrapper.c:91]: init_xml: parent element name=v:Envelope
[WARNING:mxml_wrapper.c:102]: Root element is 'v:Header', not Envelope - this may indicate a parsing issue
[DEBUG:mxml_wrapper.c:110]: Parent is Envelope, correcting root_xml from v:Header to v:Envelope
[DEBUG:mxml_wrapper.c:103]: get_method: skip_prefix=1, root_xml=0x773c6fa0
[DEBUG:mxml_wrapper.c:124]: get_method: Found method in Body: tds:GetCapabilities
```

**Analysis:**
- ✅ mxmlLoadString returned document node (type=9)
- ⚠️ First child was v:Header (wrong!)
- ✅ Parent is v:Envelope (correct!)
- ✅ Auto-corrected to parent
- ✅ Method found successfully

### Failed Parse (Old Behavior)

```
[DEBUG:mxml_wrapper.c:37]: init_xml: buffer=0x773c0470, size=933
[DEBUG:mxml_wrapper.c:45]: init_xml: XML content (first 200 chars): <v:Envelope xmlns:i="..."
[DEBUG:mxml_wrapper.c:67]: XML parsed successfully, root_xml=0x773c6fa0 (element: v:Header)
[DEBUG:mxml_wrapper.c:103]: get_method: skip_prefix=1, root_xml=0x773c6fa0
[ERROR:mxml_wrapper.c:182]: get_method: Could not find Body element or method
[FATAL:onvif_simple_server.c:314]: XML parsing error
```

**Analysis:**
- ❌ root_xml points to v:Header (wrong!)
- ❌ No validation or correction
- ❌ get_method fails to find Body
- ❌ Server exits with error

## Testing Your Fix

### 1. Build with Debug Logging

```bash
./build.sh
```

### 2. Test with Problematic XML

```bash
# Use the test script
chmod +x tests/test_xml_parsing.sh
./tests/test_xml_parsing.sh
```

### 3. Check Logs

Look for these key indicators:

**✅ Success Indicators:**
- `XML parsed successfully, root_xml=0xXXXXXXXX (element: v:Envelope)`
- `get_method: Found method in Body: ...`
- No ERROR or FATAL messages

**⚠️ Warning Indicators (but still works):**
- `Root element is 'v:Header', not Envelope - this may indicate a parsing issue`
- `Parent is Envelope, correcting root_xml from v:Header to v:Envelope`
- These mean auto-correction kicked in

**❌ Failure Indicators:**
- `Could not find Envelope element in XML document`
- `Could not find Body element or method`
- `XML parsing error`

### 4. Test with Different XML Formats

```bash
# Test 1: Standard SOAP-ENV namespace
echo '<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetCapabilities xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>' | REQUEST_METHOD=POST ./onvif_simple_server -c /etc/onvif.json -d 5

# Test 2: Custom v: namespace
echo '<?xml version="1.0" encoding="UTF-8"?>
<v:Envelope xmlns:v="http://www.w3.org/2003/05/soap-envelope">
    <v:Header/>
    <v:Body>
        <tds:GetCapabilities xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </v:Body>
</v:Envelope>' | REQUEST_METHOD=POST ./onvif_simple_server -c /etc/onvif.json -d 5

# Test 3: No namespace
echo '<?xml version="1.0" encoding="UTF-8"?>
<Envelope>
    <Header/>
    <Body>
        <tds:GetCapabilities xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </Body>
</Envelope>' | REQUEST_METHOD=POST ./onvif_simple_server -c /etc/onvif.json -d 5
```

## Troubleshooting

### Still seeing "Could not find Body element"?

1. **Check root_xml element name in logs**
   - Should end with "Envelope"
   - If not, check for correction messages

2. **Verify XML structure**
   - Must have Envelope as root
   - Must have Body as child of Envelope
   - Method must be child of Body

3. **Check namespace consistency**
   - If Envelope uses `v:`, Body should also use `v:`
   - Mismatched namespaces can cause issues

### Memory issues or crashes?

1. **Check close_xml() is called**
   - Should be called after each request
   - Prevents memory leaks

2. **Verify mxml version**
   - Run: `pkg-config --modversion mxml` or check mxml/mxml.h
   - Different versions may behave differently

3. **Enable AddressSanitizer**
   ```bash
   make debug
   ./onvif_simple_server_debug -c /etc/onvif.json -d 5
   ```

### Need more details?

1. **Increase log level to 5 (DEBUG)**
   ```bash
   ./onvif_simple_server -c /etc/onvif.json -d 5
   ```

2. **Save full XML to file**
   ```bash
   cat > /tmp/request.xml << 'EOF'
   <your XML here>
   EOF
   cat /tmp/request.xml | REQUEST_METHOD=POST ./onvif_simple_server -c /etc/onvif.json -d 5
   ```

3. **Check mxml behavior**
   - Add more debug prints in init_xml()
   - Print all children of doc_xml
   - Print element types and names

## Summary

The fix adds:
1. ✅ mxml version detection
2. ✅ Root element validation
3. ✅ Auto-correction via parent checking
4. ✅ Explicit Envelope search
5. ✅ Namespace suffix matching
6. ✅ Enhanced debug logging
7. ✅ Proper memory management

All designed to ensure `root_xml` always points to the SOAP Envelope, regardless of:
- mxml library version
- Namespace prefix used
- XML formatting variations


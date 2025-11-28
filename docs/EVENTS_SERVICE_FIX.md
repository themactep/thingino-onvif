# Events Service Subscription ID Validation Fix

## Issue

Error message in production logs:
```
Oct  2 14:21:30 onvif_simple_server[1924]: [ERROR:events_service.c:845]: sub index out of range for Unsubscribe method
```

## Root Cause

The events service was not properly validating subscription IDs from query strings before processing them. The issue occurred in two methods:

1. **Unsubscribe** (`events_unsubscribe()`)
2. **Renew** (`events_renew()`)

### Original Code Problem

```c
sub_id_s = (char *) malloc((qs_size + 1) * sizeof(char));
strncpy(sub_id_s, qs_string, qs_size);
sub_id = atoi(sub_id_s);  // ← No validation before atoi()
free(sub_id_s);

if ((sub_id <= 0) || (sub_id > 65535)) {
    log_error("sub index out of range for Unsubscribe method");
    // ...
}
```

### Issues with Original Code

1. **No input validation**: The code didn't check if the query string contained valid numeric data
2. **Silent failures**: `atoi()` returns 0 for invalid input, which triggers the range check
3. **Poor error messages**: The error didn't indicate what the invalid value was
4. **Potential security issue**: Malformed input could cause unexpected behavior

### Scenarios That Triggered the Error

1. **Empty subscription ID**: `?sub=` → `atoi("")` returns 0
2. **Non-numeric data**: `?sub=abc` → `atoi("abc")` returns 0
3. **Special characters**: `?sub=123abc` → `atoi("123abc")` returns 123 (partial parse)
4. **Out of range**: `?sub=99999` → Valid parse but out of range

## Solution

Enhanced validation with better error handling and logging:

### New Code

```c
sub_id_s = (char *) malloc((qs_size + 1) * sizeof(char));
memset(sub_id_s, '\0', qs_size + 1);
strncpy(sub_id_s, qs_string, qs_size);

// Validate that the string contains only digits
int valid = 1;
for (i = 0; i < qs_size; i++) {
    if (sub_id_s[i] < '0' || sub_id_s[i] > '9') {
        valid = 0;
        break;
    }
}

if (!valid) {
    log_error("Invalid sub parameter (non-numeric) for Unsubscribe method: '%s'", sub_id_s);
    free(sub_id_s);
    send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", 
               "wsrf-rw:ResourceUnknownFault", "Resource unknown", 
               "Invalid subscription ID format");
    return -2;
}

sub_id = atoi(sub_id_s);
free(sub_id_s);

if ((sub_id <= 0) || (sub_id > 65535)) {
    log_error("sub index out of range for Unsubscribe method: %d (valid range: 1-65535)", sub_id);
    send_fault("events_service", "Receiver", "wsrf-rw:ResourceUnknownFault", 
               "wsrf-rw:ResourceUnknownFault", "Resource unknown", 
               "Subscription ID out of valid range");
    return -2;
}

log_debug("Unsubscribe request for subscription ID: %d", sub_id);
```

### Improvements

1. ✅ **Input validation**: Checks that the string contains only digits before parsing
2. ✅ **Better error messages**: Logs the actual invalid value received
3. ✅ **Detailed SOAP faults**: Returns specific error messages to the client
4. ✅ **Debug logging**: Logs successful subscription ID for debugging
5. ✅ **Security**: Prevents processing of malformed input

## Files Modified

- `src/events_service.c`:
  - `events_unsubscribe()` function (lines 839-868)
  - `events_renew()` function (lines 616-645)

## Testing

### Test Case 1: Valid Subscription ID
```bash
# Request: Unsubscribe with valid ID
curl -X POST "http://localhost:8000/onvif/events_service?sub=123" \
  -H "Content-Type: application/soap+xml" \
  -d '<s:Envelope>...</s:Envelope>'
```
**Expected**: Success (if subscription exists) or proper "Resource unknown" fault
**Log**: `[DEBUG] Unsubscribe request for subscription ID: 123`

### Test Case 2: Invalid (Non-numeric) Subscription ID
```bash
# Request: Unsubscribe with non-numeric ID
curl -X POST "http://localhost:8000/onvif/events_service?sub=abc" \
  -H "Content-Type: application/soap+xml" \
  -d '<s:Envelope>...</s:Envelope>'
```
**Expected**: SOAP fault with "Invalid subscription ID format"
**Log**: `[ERROR] Invalid sub parameter (non-numeric) for Unsubscribe method: 'abc'`

### Test Case 3: Out of Range Subscription ID
```bash
# Request: Unsubscribe with out-of-range ID
curl -X POST "http://localhost:8000/onvif/events_service?sub=99999" \
  -H "Content-Type: application/soap+xml" \
  -d '<s:Envelope>...</s:Envelope>'
```
**Expected**: SOAP fault with "Subscription ID out of valid range"
**Log**: `[ERROR] sub index out of range for Unsubscribe method: 99999 (valid range: 1-65535)`

### Test Case 4: Empty Subscription ID
```bash
# Request: Unsubscribe with empty ID
curl -X POST "http://localhost:8000/onvif/events_service?sub=" \
  -H "Content-Type: application/soap+xml" \
  -d '<s:Envelope>...</s:Envelope>'
```
**Expected**: SOAP fault with "Resource unknown"
**Log**: `[ERROR] No sub parameter in query string for Unsubscribe method`

## Error Messages

### Before Fix
```
[ERROR:events_service.c:845]: sub index out of range for Unsubscribe method
```
- No indication of what the invalid value was
- Hard to debug in production

### After Fix

**For non-numeric input**:
```
[ERROR:events_service.c:853]: Invalid sub parameter (non-numeric) for Unsubscribe method: 'abc'
```

**For out-of-range input**:
```
[ERROR:events_service.c:863]: sub index out of range for Unsubscribe method: 99999 (valid range: 1-65535)
```

**For successful requests** (DEBUG level):
```
[DEBUG:events_service.c:868]: Unsubscribe request for subscription ID: 123
```

## SOAP Fault Responses

### Invalid Format
```xml
<env:Fault>
  <env:Code>
    <env:Value>env:Receiver</env:Value>
    <env:Subcode>
      <env:Value>wsrf-rw:ResourceUnknownFault</env:Value>
    </env:Subcode>
  </env:Code>
  <env:Reason>
    <env:Text>Resource unknown</env:Text>
  </env:Reason>
  <env:Detail>
    <env:Text>Invalid subscription ID format</env:Text>
  </env:Detail>
</env:Fault>
```

### Out of Range
```xml
<env:Fault>
  <env:Code>
    <env:Value>env:Receiver</env:Value>
    <env:Subcode>
      <env:Value>wsrf-rw:ResourceUnknownFault</env:Value>
    </env:Subcode>
  </env:Code>
  <env:Reason>
    <env:Text>Resource unknown</env:Text>
  </env:Reason>
  <env:Detail>
    <env:Text>Subscription ID out of valid range</env:Text>
  </env:Detail>
</env:Fault>
```

## Impact

### Security
- ✅ Prevents processing of malformed input
- ✅ Validates data before conversion
- ✅ Returns proper error responses

### Debugging
- ✅ Clear error messages with actual values
- ✅ Easier to diagnose issues in production
- ✅ Debug logging for successful operations

### Compatibility
- ✅ Backward compatible with valid requests
- ✅ Better error handling for invalid requests
- ✅ Standards-compliant SOAP faults

## Production Deployment

After deploying this fix:

1. **Monitor logs** for the new error messages
2. **Investigate patterns** if you see frequent invalid subscription IDs
3. **Check client implementations** if specific clients send malformed requests
4. **Enable DEBUG logging** temporarily if you need to trace subscription operations

## Related Code

The same validation pattern should be applied to any other methods that parse numeric IDs from query strings or XML content. Consider reviewing:

- Other event service methods
- Media service methods with profile IDs
- PTZ service methods with preset IDs

## Conclusion

This fix improves the robustness and debuggability of the events service by:
- ✅ Validating input before processing
- ✅ Providing clear error messages
- ✅ Returning proper SOAP faults to clients
- ✅ Making production debugging easier

The error you saw in production should now provide more information about what went wrong, making it easier to identify and fix client-side issues.


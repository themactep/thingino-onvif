# Events Service Subscription ID Debugging and Validation Fix

## Issue Reported

Production log error:
```
Oct  2 14:21:30 onvif_simple_server[1924]: [ERROR:events_service.c:845]: sub index out of range for Unsubscribe method
```

## Investigation

### Test Request
The actual ONVIF Unsubscribe request from the client:
```
POST /onvif/events_service?sub=16 HTTP/1.1
```

With SOAP body:
```xml
<wsnt:Unsubscribe/>
```

### Debug Output
After adding debug logging, we can see the request is processed correctly:
```
[INFO:events_service.c:856]: Unsubscribe request received
[DEBUG:events_service.c:860]: QUERY_STRING environment variable: 'sub=16'
[DEBUG:events_service.c:871]: Extracted sub parameter: '16' (size=2)
[DEBUG:events_service.c:902]: Unsubscribe request for subscription ID: 16
```

The subscription ID (16) is valid and within range (1-65535).

### Root Cause Analysis

The "sub index out of range" error can occur in several scenarios:

1. **Invalid subscription ID** - Non-numeric or out of range (0, negative, > 65535)
2. **Empty query string** - `?sub=` with no value
3. **Missing query parameter** - No `sub` parameter at all
4. **Malformed query string** - Special characters or encoding issues

## Fixes Implemented

### 1. Enhanced Input Validation

Added validation to check that the subscription ID contains only digits before parsing:

```c
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
```

### 2. Improved Error Messages

**Before**:
```
[ERROR:events_service.c:845]: sub index out of range for Unsubscribe method
```

**After**:
```
[ERROR:events_service.c:877]: Invalid sub parameter (non-numeric) for Unsubscribe method: 'abc'
```
or
```
[ERROR:events_service.c:897]: sub index out of range for Unsubscribe method: 99999 (valid range: 1-65535)
```

### 3. Debug Logging

Added debug logging to trace subscription ID processing:

```c
// Debug: log the QUERY_STRING environment variable
char *query_env = getenv("QUERY_STRING");
log_debug("QUERY_STRING environment variable: '%s'", query_env ? query_env : "(null)");

// ... after extraction ...
log_debug("Extracted sub parameter: '%.*s' (size=%d)", qs_size, qs_string, qs_size);

// ... after validation ...
log_debug("Unsubscribe request for subscription ID: %d", sub_id);
```

## Files Modified

- `src/events_service.c`:
  - `events_unsubscribe()` - Lines 848-902
  - `events_renew()` - Lines 606-660
  - `events_pull_messages()` - Should also be updated (lines 239-256)
  - `events_set_synchronization_point()` - Should also be updated (lines 944-961)

## Testing

### Test Case 1: Valid Subscription ID ✅
```bash
curl -X POST "http://localhost:8000/onvif/events_service?sub=16" \
  -H "Content-Type: application/soap+xml" \
  -d '<SOAP-ENV:Envelope>...<wsnt:Unsubscribe/></SOAP-ENV:Envelope>'
```

**Result**: Subscription ID extracted correctly
**Logs**:
```
[DEBUG] QUERY_STRING environment variable: 'sub=16'
[DEBUG] Extracted sub parameter: '16' (size=2)
[DEBUG] Unsubscribe request for subscription ID: 16
```

### Test Case 2: Invalid (Non-numeric) ID
```bash
curl -X POST "http://localhost:8000/onvif/events_service?sub=abc" \
  -H "Content-Type: application/soap+xml" \
  -d '<SOAP-ENV:Envelope>...<wsnt:Unsubscribe/></SOAP-ENV:Envelope>'
```

**Expected**: SOAP fault with "Invalid subscription ID format"
**Expected Log**:
```
[ERROR] Invalid sub parameter (non-numeric) for Unsubscribe method: 'abc'
```

### Test Case 3: Out of Range ID
```bash
curl -X POST "http://localhost:8000/onvif/events_service?sub=99999" \
  -H "Content-Type: application/soap+xml" \
  -d '<SOAP-ENV:Envelope>...<wsnt:Unsubscribe/></SOAP-ENV:Envelope>'
```

**Expected**: SOAP fault with "Subscription ID out of valid range"
**Expected Log**:
```
[ERROR] sub index out of range for Unsubscribe method: 99999 (valid range: 1-65535)
```

## Production Debugging

When you see the "sub index out of range" error in production:

### 1. Enable DEBUG Logging
```json
{
    "loglevel": "DEBUG"
}
```

### 2. Check the Logs
Look for these debug messages:
```
[DEBUG:events_service.c:860]: QUERY_STRING environment variable: '...'
[DEBUG:events_service.c:871]: Extracted sub parameter: '...' (size=...)
```

### 3. Identify the Issue

**If you see**:
```
[ERROR] Invalid sub parameter (non-numeric) for Unsubscribe method: 'xyz'
```
→ Client is sending non-numeric subscription ID

**If you see**:
```
[ERROR] sub index out of range for Unsubscribe method: 99999 (valid range: 1-65535)
```
→ Client is sending out-of-range subscription ID

**If you see**:
```
[ERROR] No sub parameter in query string for Unsubscribe method (QUERY_STRING='...')
```
→ Client is not including the `sub` parameter in the URL

### 4. Common Causes

1. **Client bug** - ONVIF client implementation error
2. **Subscription expired** - Client trying to unsubscribe from expired subscription
3. **Wrong URL** - Client using wrong subscription manager URL
4. **Network issue** - URL corruption during transmission

## Additional Improvements Needed

The same validation should be applied to other event service methods:

### events_pull_messages() (Line 239)
Currently has basic validation but could benefit from the same enhancements.

### events_set_synchronization_point() (Line 944)
Currently has basic validation but could benefit from the same enhancements.

## Recommendation

After deploying this fix:

1. **Monitor production logs** for the new detailed error messages
2. **Identify patterns** - Are specific clients sending invalid IDs?
3. **Check subscription lifecycle** - Are subscriptions being created/renewed properly?
4. **Verify onvif_notify_server** - Is it running and managing subscriptions correctly?

## Related Issues

The actual error in the test was:
```
[ERROR:utils.c:133]: shm_open() failed
[ERROR:events_service.c:906]: No shared memory found, is onvif_notify_server running?
```

This indicates that `onvif_notify_server` is not running, which is required for event subscriptions to work. The subscription ID validation is working correctly, but the subscription management system needs the notify server to be running.

## Conclusion

The subscription ID validation has been enhanced with:
- ✅ Input validation before parsing
- ✅ Detailed error messages with actual values
- ✅ Debug logging for troubleshooting
- ✅ Better SOAP fault responses

The original error you saw was likely caused by one of:
- Invalid subscription ID from client
- Empty or malformed query string
- Client bug in subscription management

With the new logging, you'll be able to identify the exact cause when it happens again.


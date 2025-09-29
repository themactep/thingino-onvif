# HTTP 401 Authentication Fix

## 🐛 Issue Identified

ONVIF clients (like ONVIF Device Manager) were receiving **HTTP 400 Bad Request** instead of **HTTP 401 Unauthorized** when authentication failed, causing connection failures.

### Error Symptoms
```
[06:10:30] INFO: WS-Security failed, trying without authentication...
[06:10:30] ERROR: Connection failed: Connection failed: HTTP 400
```

### Root Cause
The `send_authentication_error()` function in `src/fault.c` was returning:
```c
fprintf(stdout, "HTTP/1.1 400 Bad request\r\n");
```

But ONVIF clients expect **HTTP 401 Unauthorized** for authentication failures according to the ONVIF specification.

## 🔧 Fix Implemented

### Updated HTTP Status Code
Changed `src/fault.c` line 136:

```diff
- fprintf(stdout, "HTTP/1.1 400 Bad request\r\n");
+ fprintf(stdout, "HTTP/1.1 401 Unauthorized\r\n");
```

### Complete Fixed Function
```c
int send_authentication_error()
{
    long size = cat(NULL, "generic_files/AuthenticationError.xml", 0);

    // Use HTTP 401 Unauthorized for authentication errors (ONVIF standard)
    fprintf(stdout, "HTTP/1.1 401 Unauthorized\r\n");
    fprintf(stdout, "Content-type: application/soap+xml\r\n");
    fprintf(stdout, "Content-Length: %ld\r\n\r\n", size);

    return cat("stdout", "generic_files/AuthenticationError.xml", 0);
}
```

## ✅ Expected Results

### Before Fix
```
< HTTP/1.1 400 Bad request
< Content-type: application/soap+xml
< Content-Length: 533
```

### After Fix
```
< HTTP/1.1 401 Unauthorized
< Content-type: application/soap+xml
< Content-Length: 533
```

## 🎯 ONVIF Client Behavior

### With HTTP 400 (Wrong)
- Client interprets as "malformed request"
- Client gives up and reports "Connection failed"
- No retry with different authentication

### With HTTP 401 (Correct)
- Client interprets as "authentication required"
- Client may prompt for credentials
- Client may retry with different authentication
- Follows standard HTTP authentication flow

## 📋 SOAP Fault Response

The SOAP fault content remains the same (correct):
```xml
<env:Fault>
  <env:Code>
    <env:Value>env:Sender</env:Value>
    <env:Subcode>
      <env:Value>ter:NotAuthorized</env:Value>
    </env:Subcode>
  </env:Code>
  <env:Reason>
    <env:Text xml:lang="en">Sender not Authorized</env:Text>
  </env:Reason>
  <env:Detail>
    <env:Text>The action requested requires authorization and the sender is not authorized.</env:Text>
  </env:Detail>
</env:Fault>
```

Only the HTTP status code changes from 400 to 401.

## 🔍 Authentication Flow

### When Authentication is Required
1. **Client sends request** with WS-Security headers
2. **Server validates** username/password/digest
3. **If invalid**: Server returns HTTP 401 + SOAP fault
4. **If valid**: Server processes request normally

### When No Authentication Required
1. **Client sends request** without WS-Security headers
2. **Server processes** request normally (HTTP 200)
3. **Some methods** (like GetSystemDateAndTime) don't require auth

## 🧪 Testing

### Test Authentication Required
```bash
# This should return HTTP 401 (after fix)
curl -v -X POST \
  -H "Content-Type: application/soap+xml" \
  -H "SOAPAction: \"\"" \
  -d '<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd">
<soap:Header>
<wsse:Security>
<wsse:UsernameToken>
<wsse:Username>wrong</wsse:Username>
<wsse:Password>wrong</wsse:Password>
</wsse:UsernameToken>
</wsse:Security>
</soap:Header>
<soap:Body>
<tds:GetDeviceInformation xmlns:tds="http://www.onvif.org/ver10/device/wsdl"/>
</soap:Body>
</soap:Envelope>' \
  http://CAMERA_IP/onvif/device_service
```

### Test No Authentication Required
```bash
# This should return HTTP 200
curl -v -X POST \
  -H "Content-Type: application/soap+xml" \
  -H "SOAPAction: \"\"" \
  -d '<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope">
<soap:Body>
<tds:GetSystemDateAndTime xmlns:tds="http://www.onvif.org/ver10/device/wsdl"/>
</soap:Body>
</soap:Envelope>' \
  http://CAMERA_IP/onvif/device_service
```

## 🚀 Deployment

1. **Build updated binary** with the fix
2. **Deploy to camera** (replace existing binary)
3. **Restart ONVIF service** if needed
4. **Test with ONVIF client** (should now work)

## 📊 Impact

### For ONVIF Clients
- ✅ **Proper error handling** - Clients understand HTTP 401
- ✅ **Better user experience** - May prompt for credentials
- ✅ **Standard compliance** - Follows HTTP authentication standards
- ✅ **Retry logic** - Clients may retry with different auth

### For Server
- ✅ **ONVIF compliance** - Follows ONVIF specification
- ✅ **Standard HTTP** - Uses correct status codes
- ✅ **Better debugging** - Clear distinction between auth and other errors
- ✅ **Client compatibility** - Works with more ONVIF clients

## 🎉 Resolution

This fix ensures that:

1. **Authentication failures** return HTTP 401 (not HTTP 400)
2. **ONVIF clients** can properly handle authentication errors
3. **Standard compliance** with HTTP and ONVIF specifications
4. **Better user experience** with proper error handling

The fix is minimal, safe, and follows established standards for HTTP authentication error handling.

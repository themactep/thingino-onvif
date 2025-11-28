# Logging System Verification

## Status: ✅ WORKING CORRECTLY

The logging system is properly respecting the configured log level from `onvif.json`.

---

## Test Results

### Test 1: ERROR Level
**Config**: `"loglevel": "ERROR"`
**Result**: ✅ No DEBUG, INFO, or WARN messages appear
**Logs**: Empty (no errors occurred during test)

### Test 2: WARN Level  
**Config**: `"loglevel": "WARN"`
**Result**: ✅ No DEBUG or INFO messages appear
**Logs**: Empty (no warnings occurred during test)

### Test 3: INFO Level
**Config**: `"loglevel": "INFO"`
**Result**: ✅ No DEBUG messages appear
**Logs**: Empty (no INFO messages generated during normal operation)

### Test 4: DEBUG Level
**Config**: `"loglevel": "DEBUG"`
**Result**: ✅ All DEBUG messages appear
**Sample Logs**:
```
[DEBUG:onvif_simple_server.c:262]: REQUEST_METHOD: POST
[DEBUG:mxml_wrapper.c:37]: init_xml: buffer=0x555c79db9680, size=191
[DEBUG:mxml_wrapper.c:84]: XML parsed successfully, root_xml=0x555ca75cd0e0 (element: s:Envelope)
[DEBUG:onvif_simple_server.c:332]: Method: GetSystemDateAndTime
```

---

## How It Works

### Log Level Hierarchy
```
0 = FATAL   (lowest - only fatal errors)
1 = ERROR   (errors and fatal)
2 = WARN    (warnings, errors, and fatal)
3 = INFO    (info, warnings, errors, and fatal)
4 = DEBUG   (debug and all above)
5 = TRACE   (highest - all messages)
```

### Configuration
In `onvif.json`:
```json
{
    "loglevel": "ERROR"
}
```

Valid values: `"FATAL"`, `"ERROR"`, `"WARN"`, `"INFO"`, `"DEBUG"`, `"TRACE"` or numeric `0-5`

### Implementation
The logging system (`src/log.c`) properly filters messages:

```c
void log_log(int level, const char *file, int line, const char *fmt, ...)
{
    if (level > G.max_level)
        return; // Filter out messages above configured level
    
    // ... log the message
}
```

### Initialization Flow
1. Server starts with default level (FATAL)
2. Config file is loaded (`src/conf.c`)
3. Log level is updated (`log_set_level()` in `src/onvif_simple_server.c:256`)
4. All subsequent log calls respect the configured level

---

## Production Recommendations

### For Production Deployments
```json
{
    "loglevel": "ERROR"
}
```
- Minimal logging
- Only errors and fatal messages
- Reduces syslog noise

### For Debugging Issues
```json
{
    "loglevel": "DEBUG"
}
```
- Detailed logging
- Shows request processing flow
- Helps diagnose problems

### For Development
```json
{
    "loglevel": "TRACE"
}
```
- Maximum verbosity
- All debug information
- Performance analysis

---

## Verification Commands

### Check Current Log Level
```bash
podman exec oss cat /etc/onvif.json | grep loglevel
```

### Test with Different Levels
```bash
# Edit res/onvif.json
sed -i 's/"loglevel": ".*"/"loglevel": "ERROR"/' res/onvif.json

# Rebuild and restart
./container_build.sh
podman stop oss && podman rm oss
./container_run.sh

# Send test request
curl -X POST -H "Content-Type: application/soap+xml" \
  -d '<?xml version="1.0"?><s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><s:Body><tds:GetSystemDateAndTime/></s:Body></s:Envelope>' \
  http://localhost:8000/onvif/device_service

# Check logs
podman logs oss
```

---

## Conclusion

The logging system is working correctly and respecting the configured log level. No fixes are needed.

**Key Points**:
- ✅ Log level from config is properly loaded
- ✅ Messages are filtered based on configured level
- ✅ Production deployments can use ERROR level for minimal logging
- ✅ Debug level provides detailed information when needed
- ✅ No unnecessary syslog flooding

The system is production-ready with proper log level control.


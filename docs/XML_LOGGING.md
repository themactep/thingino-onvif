# Raw XML Request/Response Logging

## Overview

The ONVIF server includes a comprehensive XML logging feature for debugging purposes. This feature logs both incoming SOAP requests and outgoing SOAP responses to external storage, organized by client IP address.

## Purpose

This feature is designed to help developers and system administrators:
- Debug ONVIF client/server communication issues
- Analyze SOAP request/response patterns
- Troubleshoot authentication problems
- Capture real-world ONVIF traffic for testing
- Investigate compatibility issues with specific ONVIF clients

## Configuration

### Enable XML Logging

To enable XML logging, you need to:

1. **Set the log level to DEBUG (5)** - XML logging only works when debug logging is enabled
2. **Configure the raw_log_directory** - Specify a path to external storage

### Configuration Parameter

Add the following parameter to your `onvif.json` configuration file:

```json
{
    "loglevel": "DEBUG",
    "raw_log_directory": "/mnt/nfs/onvif_logs"
}
```

**Parameter Details:**
- **`raw_log_directory`**: Path to a directory on external storage (NFS mount, USB drive, etc.)
  - Must be an absolute path
  - Directory must exist and be writable
  - Leave empty (`""`) or omit to disable XML logging
  - Example paths:
    - `/mnt/nfs/onvif_logs` - NFS mount
    - `/mnt/usb/onvif_logs` - USB drive
    - `/tmp/onvif_logs` - Temporary storage (not recommended for production)

### Requirements

For XML logging to be enabled, ALL of the following conditions must be met:

1. ✅ Log level must be DEBUG (5) or higher
2. ✅ `raw_log_directory` must be configured (non-empty string)
3. ✅ Directory must exist
4. ✅ Directory must be writable by the ONVIF server process

If any condition is not met, XML logging will be automatically disabled without affecting normal server operation.

## Directory Structure

XML logs are organized by client IP address for easy analysis:

```
/mnt/nfs/onvif_logs/
├── 192.168.1.100/
│   ├── 20251002_143052_request.xml
│   ├── 20251002_143052_response.xml
│   ├── 20251002_143105_request.xml
│   ├── 20251002_143105_response.xml
│   └── ...
├── 192.168.1.101/
│   ├── 20251002_144230_request.xml
│   ├── 20251002_144230_response.xml
│   └── ...
└── unknown/
    └── ... (requests from clients without REMOTE_ADDR)
```

### Directory Organization

- **Base Directory**: The path specified in `raw_log_directory`
- **IP Subdirectories**: Automatically created for each unique client IP address
  - Named after the client's IP address (e.g., `192.168.1.100`)
  - Invalid characters in IP addresses are replaced with underscores
  - IPv6 addresses are supported
- **Unknown Directory**: Used when client IP cannot be determined

## File Naming Convention

Each request/response pair is logged to separate files with matching timestamps:

**Format**: `YYYYMMDD_HHMMSS_<type>.xml`

**Components**:
- `YYYYMMDD`: Date (e.g., `20251002` for October 2, 2025)
- `HHMMSS`: Time (e.g., `143052` for 14:30:52)
- `<type>`: Either `request` or `response`

**Examples**:
- `20251002_143052_request.xml` - SOAP request received at 14:30:52
- `20251002_143052_response.xml` - SOAP response sent at 14:30:52

**Note**: The request and its corresponding response share the same timestamp, making it easy to match them.

## File Contents

### Request Files

Request files contain the complete raw XML SOAP request as received from the client:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header>
        <wsse:Security xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd">
            <wsse:UsernameToken>
                <wsse:Username>thingino</wsse:Username>
                <wsse:Password Type="...">...</wsse:Password>
                <wsse:Nonce>...</wsse:Nonce>
                <wsu:Created>...</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        <tds:GetDeviceInformation/>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
```

### Response Files

Response files contain the complete raw XML SOAP response sent to the client:

```xml
<?xml version="1.0" ?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Body>
        <tds:GetDeviceInformationResponse>
            <tds:Manufacturer>Manufacturer</tds:Manufacturer>
            <tds:Model>Model</tds:Model>
            <tds:FirmwareVersion>0.0.1</tds:FirmwareVersion>
            <tds:SerialNumber>SN1234567890</tds:SerialNumber>
            <tds:HardwareId>HWID</tds:HardwareId>
        </tds:GetDeviceInformationResponse>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
```

**Note**: HTTP headers are NOT included in the logged files - only the XML body is captured.

## Behavior and Error Handling

### Automatic Disabling

XML logging will be automatically disabled (without affecting server operation) if:

- Log level is not DEBUG or higher
- `raw_log_directory` is not configured or is empty
- Directory does not exist
- Directory is not writable
- Disk is full (warning logged, request continues)

### Graceful Degradation

If XML logging fails for any reason:
- A warning is logged to the system log
- The ONVIF request continues to be processed normally
- The server does NOT crash or fail the request
- Subsequent requests will continue to attempt logging

### Performance Impact

When XML logging is **disabled**:
- Zero performance impact
- Single check at initialization

When XML logging is **enabled**:
- Minimal performance impact
- File I/O is performed after response is sent
- Does not block request processing
- Typical overhead: < 1ms per request

## Usage Examples

### Example 1: Enable XML Logging to NFS Mount

```json
{
    "loglevel": "DEBUG",
    "raw_log_directory": "/mnt/nfs/onvif_logs"
}
```

### Example 2: Enable XML Logging to USB Drive

```json
{
    "loglevel": "DEBUG",
    "raw_log_directory": "/mnt/usb/onvif_debug"
}
```

### Example 3: Disable XML Logging

```json
{
    "loglevel": "INFO",
    "raw_log_directory": ""
}
```

Or simply omit the `raw_log_directory` parameter.

## Analyzing Logs

### Finding Logs for a Specific Client

```bash
# List all logs from a specific IP
ls -lh /mnt/nfs/onvif_logs/192.168.1.100/

# View a specific request
cat /mnt/nfs/onvif_logs/192.168.1.100/20251002_143052_request.xml

# View the corresponding response
cat /mnt/nfs/onvif_logs/192.168.1.100/20251002_143052_response.xml
```

### Finding Recent Logs

```bash
# Find most recent logs
find /mnt/nfs/onvif_logs -name "*.xml" -type f -mtime -1 | sort

# Count logs per IP
for dir in /mnt/nfs/onvif_logs/*/; do
    echo "$(basename $dir): $(ls $dir/*.xml 2>/dev/null | wc -l) files"
done
```

### Searching for Specific Methods

```bash
# Find all GetDeviceInformation requests
grep -r "GetDeviceInformation" /mnt/nfs/onvif_logs/*/

# Find all authentication failures
grep -r "Unauthorized" /mnt/nfs/onvif_logs/*/
```

## Maintenance

### Disk Space Management

Since XML logs can accumulate quickly, implement a cleanup strategy:

```bash
# Delete logs older than 7 days
find /mnt/nfs/onvif_logs -name "*.xml" -type f -mtime +7 -delete

# Delete logs older than 24 hours
find /mnt/nfs/onvif_logs -name "*.xml" -type f -mtime +1 -delete

# Keep only the last 100 files per IP
for dir in /mnt/nfs/onvif_logs/*/; do
    ls -t $dir/*.xml | tail -n +101 | xargs rm -f
done
```

### Automated Cleanup (Cron)

Add to crontab:

```bash
# Clean up XML logs older than 7 days, daily at 2 AM
0 2 * * * find /mnt/nfs/onvif_logs -name "*.xml" -type f -mtime +7 -delete
```

## Troubleshooting

### XML Logging Not Working

Check the following:

1. **Verify log level**:
   ```bash
   grep loglevel /etc/onvif.json
   # Should show: "loglevel": "DEBUG" or "loglevel": 5
   ```

2. **Verify directory configuration**:
   ```bash
   grep raw_log_directory /etc/onvif.json
   # Should show a valid path
   ```

3. **Check directory exists and is writable**:
   ```bash
   ls -ld /mnt/nfs/onvif_logs
   touch /mnt/nfs/onvif_logs/test && rm /mnt/nfs/onvif_logs/test
   ```

4. **Check system logs**:
   ```bash
   logread | grep "XML logging"
   # Look for initialization and error messages
   ```

### Common Issues

**Issue**: "XML logging disabled: raw_log_directory not configured"
- **Solution**: Add `raw_log_directory` parameter to `onvif.json`

**Issue**: "XML logging disabled: raw_log_directory '/path' does not exist"
- **Solution**: Create the directory: `mkdir -p /path`

**Issue**: "XML logging disabled: raw_log_directory '/path' is not writable"
- **Solution**: Fix permissions: `chmod 755 /path`

**Issue**: "Response buffer overflow, XML response logging may be incomplete"
- **Solution**: This is a warning that the response was larger than 64KB. The log file will contain a truncated response. This is rare and usually indicates an unusually large SOAP response.

## Security Considerations

### Sensitive Data

XML logs may contain sensitive information:
- Usernames and password digests
- Authentication tokens
- Device configuration details
- Network information

**Recommendations**:
- Store logs on secure, access-controlled storage
- Implement log rotation and cleanup
- Do not enable XML logging in production unless necessary
- Review and sanitize logs before sharing

### Access Control

Ensure proper file permissions:

```bash
# Set restrictive permissions on log directory
chmod 700 /mnt/nfs/onvif_logs
chown onvif:onvif /mnt/nfs/onvif_logs
```

## Implementation Details

### Files Modified

- `src/onvif_simple_server.h` - Added `raw_log_directory` to service context
- `src/onvif_simple_server.c` - Integrated XML logging calls
- `src/conf.c` - Added configuration loading for `raw_log_directory`
- `src/xml_logger.h` - XML logging interface
- `src/xml_logger.c` - XML logging implementation
- `src/utils.h` - Response buffer interface
- `src/utils.c` - Response buffer implementation
- `Makefile` - Added xml_logger.o to build
- `res/onvif.json` - Added example configuration

### Architecture

The XML logging system consists of two main components:

1. **Request Logging** (`xml_logger.c`):
   - Logs raw XML request immediately after reading from stdin
   - Creates IP-specific subdirectories
   - Generates timestamp for request/response pair

2. **Response Buffering** (`utils.c`):
   - Captures response data as it's written to stdout
   - Buffers up to 64KB of response data
   - Flushes buffer to log file at end of request

Both components are designed to fail gracefully without affecting normal server operation.



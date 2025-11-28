# XML Logging - Quick Start Guide

## Enable XML Logging (3 Steps)

### Step 1: Create Log Directory

```bash
mkdir -p /mnt/nfs/onvif_logs
chmod 755 /mnt/nfs/onvif_logs
```

### Step 2: Edit Configuration

Edit `/etc/onvif.json`:

```json
{
    "loglevel": "DEBUG",
    "raw_log_directory": "/mnt/nfs/onvif_logs"
}
```

### Step 3: Restart Server

```bash
killall onvif_simple_server
# Server will restart automatically via CGI
```

## Verify It's Working

```bash
# Check system log
logread | grep "XML logging"

# Should see:
# XML logging enabled: raw_log_directory='/mnt/nfs/onvif_logs'

# Wait for a request, then check for log files
ls -lh /mnt/nfs/onvif_logs/*/
```

## View Logs

```bash
# List all logs
find /mnt/nfs/onvif_logs -name "*.xml"

# View a request
cat /mnt/nfs/onvif_logs/192.168.1.100/20251002_143052_request.xml

# View the response
cat /mnt/nfs/onvif_logs/192.168.1.100/20251002_143052_response.xml
```

## Disable XML Logging

Edit `/etc/onvif.json`:

```json
{
    "loglevel": "INFO",
    "raw_log_directory": ""
}
```

Or simply set `loglevel` to anything below DEBUG.

## Clean Up Old Logs

```bash
# Delete logs older than 7 days
find /mnt/nfs/onvif_logs -name "*.xml" -type f -mtime +7 -delete

# Delete all logs
rm -rf /mnt/nfs/onvif_logs/*
```

## Troubleshooting

### Logs Not Being Created?

1. **Check log level**:
   ```bash
   grep loglevel /etc/onvif.json
   # Must be "DEBUG" or 5
   ```

2. **Check directory**:
   ```bash
   ls -ld /mnt/nfs/onvif_logs
   # Must exist and be writable
   ```

3. **Check system log**:
   ```bash
   logread | grep "XML logging"
   # Look for error messages
   ```

### Common Errors

| Error Message | Solution |
|--------------|----------|
| "XML logging disabled: debug level not enabled" | Set `loglevel` to "DEBUG" |
| "XML logging disabled: raw_log_directory not configured" | Add `raw_log_directory` to config |
| "XML logging disabled: directory does not exist" | Create the directory |
| "XML logging disabled: directory is not writable" | Fix permissions: `chmod 755 /path` |

## File Format

### Request File
```
/mnt/nfs/onvif_logs/192.168.1.100/20251002_143052_request.xml
```

Contains the complete SOAP request XML.

### Response File
```
/mnt/nfs/onvif_logs/192.168.1.100/20251002_143052_response.xml
```

Contains the complete SOAP response XML.

**Note**: Request and response share the same timestamp.

## Automated Cleanup (Recommended)

Add to crontab:

```bash
crontab -e

# Add this line to delete logs older than 7 days, daily at 2 AM
0 2 * * * find /mnt/nfs/onvif_logs -name "*.xml" -type f -mtime +7 -delete
```

## Security Warning

⚠️ **XML logs contain sensitive information**:
- Usernames and password digests
- Authentication tokens
- Device configuration

**Recommendations**:
- Only enable when debugging
- Restrict access to log directory
- Implement log rotation
- Don't share logs publicly

## Performance

- **When disabled**: Zero overhead
- **When enabled**: < 1ms per request
- **Buffer size**: 64KB (sufficient for most responses)

## More Information

For complete documentation, see: `docs/XML_LOGGING.md`


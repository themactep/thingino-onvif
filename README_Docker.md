# ONVIF Simple Server Docker Test Environment

ONVIF Simple Server source files contain a complete Docker-based test environment.
The setup configures the ONVIF server to run as CGI applications behind a lighttpd web server.

## 🏗️ Architecture

```
┌─────────────────────────────────────────┐
│              Docker Container           │
│  ┌─────────────────────────────────────┐ │
│  │         lighttpd (port 8000)        │ │
│  │                                     │ │
│  │  /onvif/device_service    ──────────┼─┼─► onvif_simple_server (CGI)
│  │  /onvif/media_service     ──────────┼─┼─► onvif_simple_server (CGI)
│  │  /onvif/media2_service    ──────────┼─┼─► onvif_simple_server (CGI)
│  │  /onvif/ptz_service       ──────────┼─┼─► onvif_simple_server (CGI)
│  │  /onvif/events_service    ──────────┼─┼─► onvif_simple_server (CGI)
│  │  /onvif/deviceio_service  ──────────┼─┼─► onvif_simple_server (CGI)
│  └─────────────────────────────────────┘ │
└─────────────────────────────────────────┘
```

## 📁 Directory Structure

```
/
├── Dockerfile                    # Container definition
├── lighttpd.conf                # Web server configuration
├── docker_build.sh              # Build script
├── docker_run.sh                # Run script
├── docker_inspect.sh            # Container inspection
├── test_onvif_server.sh         # Test suite
├── README_Docker.md             # This documentation
├── onvif_simple_server.conf     # ONVIF server configuration
│
├── device_service_files/        # Device service XML templates
├── media_service_files/         # Media service XML templates
├── media2_service_files/        # Media2 service XML templates
├── ptz_service_files/           # PTZ service XML templates
├── events_service_files/        # Events service XML templates
├── deviceio_service_files/      # Device I/O service XML templates
├── generic_files/               # Generic response templates
├── notify_files/                # Notification server templates
└── wsd_files/                   # WS-Discovery templates
```

## 🚀 Quick Start

### 1. Build the Docker Image

```bash
./docker_build.sh
```

This script will:
- Build the ONVIF server binaries using the main build script
- Build the Docker image tagged as 'oss'

### 2. Run the Container

```bash
./docker_run.sh
```

This starts the container with:
- Port 8000 exposed for HTTP access
- lighttpd serving ONVIF services as CGI applications

### 3. Test the Server

```bash
./test_onvif_server.sh
```

This runs a comprehensive test suite that verifies:
- HTTP connectivity
- CGI execution
- ONVIF service responses

## 🔧 Configuration

### lighttpd Configuration

The `lighttpd.conf` file configures:

- **Document Root**: `/app/` (container working directory)
- **Port**: 8000
- **CGI Module**: Enabled for `.cgi` files
- **URL Aliases**: Map ONVIF service URLs to CGI scripts

### ONVIF Server Configuration

The `onvif_simple_server.conf` file contains:

- Device information (model, manufacturer, firmware)
- Media profiles (resolution, codecs, URLs)
- PTZ configuration
- Event definitions
- Authentication settings (optional)

### Template Files

Template files are organized by service type:

- **device_service_files/**: Device management operations
- **media_service_files/**: Media streaming operations
- **ptz_service_files/**: Pan-Tilt-Zoom operations
- **events_service_files/**: Event subscription operations

## 🧪 Testing

### Manual Testing

Test individual services using curl:

```bash
# Test GetSystemDateAndTime
curl -X POST \
  -H "Content-Type: application/soap+xml; charset=utf-8" \
  -d '<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope"
               xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
  <soap:Header/>
  <soap:Body>
    <tds:GetSystemDateAndTime/>
  </soap:Body>
</soap:Envelope>' \
  http://localhost:8000/onvif/device_service
```

### Automated Testing

Use the provided test script:

```bash
./test_onvif_server.sh
```

### Container Inspection

Access the running container:

```bash
./docker_inspect.sh
```

View container logs:

```bash
docker logs oss
```

## 🔍 Troubleshooting

### Common Issues

1. **Build Failures**
   - Ensure build dependencies are installed
   - Check that `./build.sh` completes successfully

2. **CGI Execution Errors**
   - Verify file permissions on CGI binaries
   - Check lighttpd error logs: `docker logs oss`

3. **Template File Errors**
   - Ensure template files are copied to correct locations
   - Verify directory structure matches expectations

4. **Network Connectivity**
   - Confirm port 8000 is not in use by other services
   - Check firewall settings

### Debug Mode

Enable debug logging by modifying the configuration or using debug files:

```bash
# Inside container
echo "5" > /tmp/onvif_simple_server.debug
```

## 📚 ONVIF Service Endpoints

| Service | URL | Description |
|---------|-----|-------------|
| Device | `/onvif/device_service` | Device management and capabilities |
| Media | `/onvif/media_service` | Media profiles and streaming |
| Media2 | `/onvif/media2_service` | Enhanced media operations |
| PTZ | `/onvif/ptz_service` | Pan-Tilt-Zoom control |
| Events | `/onvif/events_service` | Event subscription and notifications |
| DeviceIO | `/onvif/deviceio_service` | Device I/O operations |

## 🔐 Security Considerations

- The test environment runs without authentication by default
- For production use, enable authentication in the configuration
- Consider using HTTPS for secure communications
- Implement proper access controls and network security

## 📝 Notes

- The container runs as a single-service test environment
- Template files are pre-configured for basic ONVIF compliance
- Modify configuration files to match your specific device capabilities
- The setup is designed for development and testing purposes

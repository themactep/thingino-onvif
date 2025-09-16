# ONVIF Simple Server Test Environment

This directory contains a complete test environment for the ONVIF Simple Server, allowing you to test ONVIF functionality locally before deploying to embedded devices.

## Quick Start

### 1. Build the Server
```bash
# From project root
make clean && make
```

### 2. Start Test Server
```bash
# Start HTTP CGI server on port 8080
./server/start_test_server.sh
```

### 3. Run Tests
```bash
# In another terminal, run the test suite
./server/tests/run_tests.sh
```

## Test Methods

### Method 1: HTTP CGI Server (Recommended)
Simulates a real web server environment:

```bash
# Terminal 1: Start server
./server/start_test_server.sh

# Terminal 2: Run tests
./server/tests/run_tests.sh
```

### Method 2: Direct Testing
Tests the binary directly without HTTP server:

```bash
./server/tests/test_direct.sh
```

## Available Tests

### SOAP Test Requests
- **GetCapabilities**: Tests device capabilities including relay count
- **GetServices**: Tests service discovery with capabilities
- **DeviceIO GetServiceCapabilities**: Tests DeviceIO service capabilities
- **DeviceIO GetRelayOutputs**: Tests relay output enumeration

### Test Files
- `tests/test_getcapabilities.xml` - Device GetCapabilities request
- `tests/test_getservices.xml` - Device GetServices request  
- `tests/test_deviceio_getcapabilities.xml` - DeviceIO GetServiceCapabilities request
- `tests/test_deviceio_getrelayoutputs.xml` - DeviceIO GetRelayOutputs request

## Configuration Testing

The test environment uses the same configuration system as the production server:

### Relay Configuration
Edit `config/relays.json` to test different relay counts:

```json
[
  {
    "idle_state": "close",
    "close": "ircut off", 
    "open": "ircut on"
  },
  {
    "idle_state": "close",
    "close": "irled off",
    "open": "irled on"
  }
]
```

### Server Configuration
The test server uses `server/cgi-bin/onvif.json` which points to `../config` for modular configuration.

## Expected Results

### Successful Test Output
```
🧪 ONVIF Simple Server Test Suite
==================================

📋 Configuration Information
==============================
Relays in config: 2
Relay details:
  - ircut off / ircut on (idle: close)
  - irled off / irled on (idle: close)

🚀 Running ONVIF Tests
=====================

Testing: Device GetCapabilities
Service: device_service
Request: test_getcapabilities.xml
✅ PASS: Response contains expected pattern
📊 RelayOutputs: 2
📊 VideoSources: 1
📊 AudioSources: 1

📊 Test Results Summary
=======================
Tests passed: 4
Tests failed: 0
Total tests: 4

🎉 All tests passed!
```

### Key Metrics to Verify
- **RelayOutputs**: Should match the number of relays in `config/relays.json`
- **VideoSources**: Should be 1 (from profiles configuration)
- **AudioSources**: Should be 1 (if audio is configured)

## Troubleshooting

### Server Won't Start
- Check if port 8080 is already in use: `lsof -i :8080`
- Ensure the binary is built: `make clean && make`

### Tests Fail
- Check server logs for debug messages
- Verify configuration files exist and are valid JSON
- Ensure XML template files are present

### No Relay Count
- Check `config/relays.json` exists and is valid
- Look for debug messages: `DEBUG: Found X relay entries`
- Verify `conf_dir` in configuration points to correct directory

## Manual Testing

You can also test manually with curl:

```bash
# Test GetCapabilities
curl -X POST \
  -H "Content-Type: application/soap+xml" \
  -d @server/tests/test_getcapabilities.xml \
  http://localhost:8080/cgi-bin/device_service

# Extract relay count
curl -s -X POST \
  -H "Content-Type: application/soap+xml" \
  -d @server/tests/test_getcapabilities.xml \
  http://localhost:8080/cgi-bin/device_service | \
  grep -o '<tt:RelayOutputs>[^<]*</tt:RelayOutputs>'
```

## Integration with TinyCam

This test environment helps verify that your ONVIF server will work correctly with TinyCam Pro:

1. **Relay Detection**: TinyCam checks `GetCapabilities` for `<tt:RelayOutputs>` count
2. **Service Discovery**: TinyCam uses `GetServices` to find available services
3. **DeviceIO**: TinyCam uses DeviceIO service for relay control

If tests pass here, TinyCam should detect your relay outputs correctly!

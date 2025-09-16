# Enhanced HTTP Communication Logging for ONVIF Tests

## Overview

The ONVIF test script `tests/run_comprehensive_tests.sh` has been enhanced with detailed HTTP communication logging to improve debugging capabilities and provide better visibility into SOAP protocol exchanges.

## Enhanced Features

### 1. **Complete Request URLs**
- Displays the full URL being called for each ONVIF service request
- Format: `🌐 Request URL: http://server:port/onvif/service_name`

### 2. **Raw XML Request Logging**
- Shows the complete SOAP XML request body being sent
- Automatically formatted using `xmllint` for better readability
- Includes authentication headers when applicable
- Clearly separated with visual dividers

### 3. **Raw XML Response Logging**
- Displays the complete SOAP XML response received from the server
- Formatted for easy reading and analysis
- Shows error responses and fault details
- Helps verify that named profiles and other configuration changes are reflected in responses

### 4. **HTTP Status Code Display**
- Shows the HTTP status code for each request
- Helps identify connection issues vs. application-level errors

### 5. **Enhanced Test Evaluation**
- Clear indication of expected patterns being tested
- Detailed pass/fail reporting with reasons
- Maintains all existing test logic and validation

### 6. **Response Highlights**
- Extracts and displays key ONVIF elements from responses
- Shows RelayOutputs, VideoSources, Manufacturer, Model, etc.
- Provides quick overview of important configuration values

## Debug Mode Control

The enhanced logging can be controlled via the `DEBUG_MODE` environment variable:

```bash
# Enable full logging (default)
DEBUG_MODE=true ./tests/run_comprehensive_tests.sh

# Disable detailed XML logging for cleaner output
DEBUG_MODE=false ./tests/run_comprehensive_tests.sh
```

## Example Output

```
Testing: Device GetCapabilities
Service: device_service
Request: test_getcapabilities.xml
🌐 Request URL: http://192.168.88.201:80/onvif/device_service

📤 Raw XML Request:
----------------------------------------
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
  <soap:Header/>
  <soap:Body>
    <tds:GetCapabilities>
       <tds:Category>All</tds:Category>
    </tds:GetCapabilities>
  </soap:Body>
</soap:Envelope>
----------------------------------------

🔧 cURL Command: curl -s -w "%{http_code}" -X POST -H 'Content-Type: application/soap+xml' -H 'SOAPAction: ""' -d @"/tmp/tmp.xyz" http://192.168.88.201:80/onvif/device_service

📊 HTTP Status Code: 200

📥 Raw XML Response:
========================================
<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema">
  <SOAP-ENV:Body>
    <tds:GetCapabilitiesResponse>
      <tds:Capabilities>
        <tt:Device>
          <tt:XAddr>http://192.168.88.201:80/onvif/device_service</tt:XAddr>
          <!-- ... more XML content ... -->
        </tt:Device>
      </tds:Capabilities>
    </tds:GetCapabilitiesResponse>
  </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
========================================

🔍 Test Evaluation:
Expected pattern: RelayOutputs
✅ PASS: Response contains expected pattern

📋 Response Highlights:
📊 RelayOutputs: 2
📊 Manufacturer: Test Manufacturer
📊 Model: Test Camera

==================================================
```

## Benefits for Named Profiles Testing

The enhanced logging is particularly useful for verifying the named profiles configuration changes:

1. **Request Verification**: Confirms that profile-related requests are properly formatted
2. **Response Analysis**: Shows how the server responds with profile information
3. **Configuration Validation**: Verifies that named profiles (stream0, stream1) are correctly reflected in SOAP responses
4. **Debugging**: Helps identify issues with profile access patterns and URL generation

## Usage

Simply run the enhanced test script as usual:

```bash
# Run all tests with enhanced logging
./tests/run_comprehensive_tests.sh

# Run with minimal logging
DEBUG_MODE=false ./tests/run_comprehensive_tests.sh
```

The script maintains full backward compatibility while providing significantly improved debugging capabilities for ONVIF protocol development and troubleshooting.

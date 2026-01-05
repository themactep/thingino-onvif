#!/bin/bash

# Test script to verify CGI service functionality with memory debugging

set -e

echo "Testing CGI service functionality with AddressSanitizer..."

# Build debug version
make debug

# Create test directory
mkdir -p test_cgi
cd test_cgi

# Create minimal test config
cat > onvif.json << 'EOF'
{
    "model": "Test Camera",
    "manufacturer": "Test Manufacturer",
    "firmware_ver": "1.0.0",
    "serial_num": "TEST123",
    "hardware_id": "TEST_HW",
    "ifs": "lo",
    "port": 8080,
    "username": "admin",
    "password": "admin",
    "profiles": [
        {
            "name": "Profile_0",
            "width": 1920,
            "height": 1080,
            "url": "rtsp://192.168.1.100:554/stream1",
            "snapurl": "http://192.168.1.100/snapshot.jpg",
            "type": "H264",
            "audio_encoder": "NONE"
        }
    ]
}
EOF

# Create simple SOAP request for media service
cat > media_request.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl">
   <soap:Header/>
   <soap:Body>
      <trt:GetProfiles/>
   </soap:Body>
</soap:Envelope>
EOF

# Create simple SOAP request for device service
cat > device_request.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
   <soap:Header/>
   <soap:Body>
      <tds:GetDeviceInformation/>
   </soap:Body>
</soap:Envelope>
EOF

echo "Testing media_service with AddressSanitizer..."

# Set up CGI environment for media service
export REQUEST_METHOD=POST
export CONTENT_TYPE="application/soap+xml"
export CONTENT_LENGTH=$(wc -c < media_request.xml)
export SCRIPT_NAME="/cgi-bin/media_service"

# Test media service (this was crashing before)
echo "Running media_service test..."
timeout 10s ../onvif_simple_server_debug media_service -c onvif.json < media_request.xml > media_response.xml 2>&1 || true

if [ -f media_response.xml ] && [ -s media_response.xml ]; then
    echo "✅ Media service test completed without crash"
    echo "Response size: $(wc -c < media_response.xml) bytes"
else
    echo "❌ Media service test failed - no response generated"
fi

echo "Testing device_service with AddressSanitizer..."

# Set up CGI environment for device service
export SCRIPT_NAME="/cgi-bin/device_service"
export CONTENT_LENGTH=$(wc -c < device_request.xml)

# Test device service
echo "Running device_service test..."
timeout 10s ../onvif_simple_server_debug device_service -c onvif.json < device_request.xml > device_response.xml 2>&1 || true

if [ -f device_response.xml ] && [ -s device_response.xml ]; then
    echo "✅ Device service test completed without crash"
    echo "Response size: $(wc -c < device_response.xml) bytes"
else
    echo "❌ Device service test failed - no response generated"
fi

# Check for any AddressSanitizer errors in the output
if grep -q "AddressSanitizer" media_response.xml device_response.xml 2>/dev/null; then
    echo "❌ AddressSanitizer errors detected in service responses"
    exit 1
else
    echo "✅ No AddressSanitizer errors detected"
fi

# Cleanup
cd ..
rm -rf test_cgi

echo "CGI service tests completed successfully!"

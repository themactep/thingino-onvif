#!/bin/bash

# Test script to verify unified configuration system

set -e

echo "Testing unified configuration system..."

# Build debug version
make debug

echo "Testing unified configuration loading..."

# Test with unified configuration file
export REQUEST_METHOD=POST
export CONTENT_TYPE="application/soap+xml"
export SCRIPT_NAME="/cgi-bin/device_service"

# Create simple SOAP request for device service  
cat > test_device_request.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
   <soap:Header/>
   <soap:Body>
      <tds:GetDeviceInformation/>
   </soap:Body>
</soap:Envelope>
EOF

export CONTENT_LENGTH=$(wc -c < test_device_request.xml)

echo "Running device service with unified configuration..."
timeout 10s ./onvif_simple_server_debug device_service -c test_config/unified_onvif.json < test_device_request.xml > unified_device_response.xml 2>&1 || true

if [ -f unified_device_response.xml ] && [ -s unified_device_response.xml ]; then
    echo "✅ Device service test with unified config completed without crash"
    echo "Response size: $(wc -c < unified_device_response.xml) bytes"
    
    # Check if the response contains expected device information
    if grep -q "Test Camera" unified_device_response.xml && grep -q "Test Manufacturer" unified_device_response.xml; then
        echo "✅ Device information correctly loaded from unified config"
    else
        echo "❌ Device information not found in response"
    fi
else
    echo "❌ Device service test with unified config failed - no response generated"
fi

# Test media service with profiles
export SCRIPT_NAME="/cgi-bin/media_service"

cat > test_media_request.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:trt="http://www.onvif.org/ver10/media/wsdl">
   <soap:Header/>
   <soap:Body>
      <trt:GetProfiles/>
   </soap:Body>
</soap:Envelope>
EOF

export CONTENT_LENGTH=$(wc -c < test_media_request.xml)

echo "Running media service with unified configuration..."
timeout 10s ./onvif_simple_server_debug media_service -c test_config/unified_onvif.json < test_media_request.xml > unified_media_response.xml 2>&1 || true

if [ -f unified_media_response.xml ] && [ -s unified_media_response.xml ]; then
    echo "✅ Media service test with unified config completed without crash"
    echo "Response size: $(wc -c < unified_media_response.xml) bytes"
    
    # Check if the response contains expected profile information
    if grep -q "Profile_0" unified_media_response.xml && grep -q "Profile_1" unified_media_response.xml; then
        echo "✅ Profiles correctly loaded from unified config"
    else
        echo "❌ Profiles not found in response"
    fi
else
    echo "❌ Media service test with unified config failed - no response generated"
fi

# Test PTZ service
export SCRIPT_NAME="/cgi-bin/ptz_service"

cat > test_ptz_request.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:tptz="http://www.onvif.org/ver20/ptz/wsdl">
   <soap:Header/>
   <soap:Body>
      <tptz:GetServiceCapabilities/>
   </soap:Body>
</soap:Envelope>
EOF

export CONTENT_LENGTH=$(wc -c < test_ptz_request.xml)

echo "Running PTZ service with unified configuration..."
timeout 10s ./onvif_simple_server_debug ptz_service -c test_config/unified_onvif.json < test_ptz_request.xml > unified_ptz_response.xml 2>&1 || true

if [ -f unified_ptz_response.xml ] && [ -s unified_ptz_response.xml ]; then
    echo "✅ PTZ service test with unified config completed without crash"
    echo "Response size: $(wc -c < unified_ptz_response.xml) bytes"
else
    echo "❌ PTZ service test with unified config failed - no response generated"
fi

# Check for any AddressSanitizer errors in the output
if grep -q "AddressSanitizer" unified_*.xml 2>/dev/null; then
    echo "❌ AddressSanitizer errors detected in service responses"
    exit 1
else
    echo "✅ No AddressSanitizer errors detected"
fi

# Cleanup
rm -f test_device_request.xml test_media_request.xml test_ptz_request.xml
rm -f unified_device_response.xml unified_media_response.xml unified_ptz_response.xml

echo "Unified configuration tests completed successfully!"

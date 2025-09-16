#!/bin/bash

# Test script to verify configuration loading

set -e

echo "Testing configuration loading..."

# Build debug version
make debug

echo "Testing configuration loading with debug output..."

# Create proper SOAP request
cat > test_soap_request.xml << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
   <soap:Header/>
   <soap:Body>
      <tds:GetDeviceInformation/>
   </soap:Body>
</soap:Envelope>
EOF

# Set up CGI environment
export REQUEST_METHOD=POST
export CONTENT_TYPE="application/soap+xml"
export SCRIPT_NAME="/cgi-bin/device_service"
export CONTENT_LENGTH=$(wc -c < test_soap_request.xml)

echo "Running with unified configuration and capturing debug output..."

# Run and capture all debug output
./onvif_simple_server_debug device_service -c test_config/unified_onvif.json < test_soap_request.xml > config_test_output.txt 2>&1 || true

echo "Configuration loading debug output:"
echo "=================================="

# Show configuration loading messages
if grep -q "model:" config_test_output.txt; then
    echo "✅ Basic configuration loaded:"
    grep -E "(model|manufacturer|firmware_ver|serial_num|hardware_id):" config_test_output.txt
else
    echo "❌ Basic configuration not found in debug output"
fi

if grep -q "Profile" config_test_output.txt; then
    echo "✅ Profiles loaded:"
    grep "Profile" config_test_output.txt
else
    echo "❌ No profiles found in debug output"
fi

if grep -q "relay" config_test_output.txt; then
    echo "✅ Relays loaded:"
    grep -i "relay" config_test_output.txt
else
    echo "❌ No relays found in debug output"
fi

echo ""
echo "Full debug output:"
echo "=================="
cat config_test_output.txt

# Cleanup
rm -f test_soap_request.xml config_test_output.txt

echo "Configuration loading test completed!"

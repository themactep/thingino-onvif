#!/bin/bash

# Comprehensive ONVIF testing script
# Tests various ONVIF methods with proper CGI environment

ONVIF_SERVER="/mnt/nfs/onvif_simple_server"
CONFIG_FILE="/etc/onvif.json"
DEBUG_LEVEL="5"

# Common CGI environment
setup_cgi_env() {
    export REQUEST_METHOD="POST"
    export CONTENT_TYPE="application/soap+xml; charset=utf-8"
    export SERVER_NAME="192.168.1.100"  # Change to your camera's IP
    export SERVER_PORT="80"
    export SCRIPT_NAME="/onvif/device_service"
    export REQUEST_URI="/onvif/device_service"
    export SERVER_PROTOCOL="HTTP/1.1"
    export GATEWAY_INTERFACE="CGI/1.1"
    export SERVER_SOFTWARE="nginx/1.18.0"
}

# Test function
test_onvif_method() {
    local method_name="$1"
    local soap_request="$2"
    
    echo "========================================="
    echo "Testing: $method_name"
    echo "========================================="
    
    setup_cgi_env
    export CONTENT_LENGTH=${#soap_request}
    
    echo "Content-Length: $CONTENT_LENGTH"
    echo ""
    echo "Request:"
    echo "$soap_request"
    echo ""
    echo "Response:"
    echo "-----------------------------------------"
    
    echo "$soap_request" | $ONVIF_SERVER -c $CONFIG_FILE -d $DEBUG_LEVEL
    
    echo ""
    echo ""
}

# Test 1: GetCapabilities
GETCAPABILITIES='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetCapabilities>
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Test 2: GetDeviceInformation
GETDEVICEINFO='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetDeviceInformation/>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Test 3: GetServices
GETSERVICES='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetServices>
            <tds:IncludeCapability>false</tds:IncludeCapability>
        </tds:GetServices>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Run tests
echo "ONVIF Simple Server CGI Test Suite"
echo "==================================="
echo ""

test_onvif_method "GetCapabilities" "$GETCAPABILITIES"
test_onvif_method "GetDeviceInformation" "$GETDEVICEINFO"
test_onvif_method "GetServices" "$GETSERVICES"

echo "Test suite completed!"

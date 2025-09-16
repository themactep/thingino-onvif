#!/bin/sh

# Test ONVIF methods that might not require authentication

# Set CGI environment variables
export REQUEST_METHOD="POST"
export CONTENT_TYPE="application/soap+xml; charset=utf-8"
export SERVER_NAME="192.168.1.100"
export SERVER_PORT="80"
export SCRIPT_NAME="/onvif/device_service"
export REQUEST_URI="/onvif/device_service"
export SERVER_PROTOCOL="HTTP/1.1"
export GATEWAY_INTERFACE="CGI/1.1"
export SERVER_SOFTWARE="nginx/1.18.0"

test_method() {
    local method_name="$1"
    local soap_request="$2"
    
    echo "========================================="
    echo "Testing: $method_name (no authentication)"
    echo "========================================="
    
    export CONTENT_LENGTH=${#soap_request}
    echo "Content-Length: $CONTENT_LENGTH"
    echo ""
    echo "Response:"
    echo "-----------------------------------------"
    
    echo "$soap_request" | /mnt/nfs/onvif_simple_server -c /etc/onvif.json -d 5
    echo ""
}

# Test GetServices (sometimes allowed without auth)
GETSERVICES='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetServices>
            <tds:IncludeCapability>false</tds:IncludeCapability>
        </tds:GetServices>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Test GetSystemDateAndTime (usually allowed without auth)
GETSYSTEMDATETIME='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetSystemDateAndTime/>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Test GetCapabilities (sometimes allowed without auth)
GETCAPABILITIES='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetCapabilities>
            <tds:Category>Device</tds:Category>
        </tds:GetCapabilities>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

echo "Testing ONVIF methods without authentication..."
echo ""

test_method "GetSystemDateAndTime" "$GETSYSTEMDATETIME"
test_method "GetServices" "$GETSERVICES"
test_method "GetCapabilities" "$GETCAPABILITIES"

echo "Test completed!"

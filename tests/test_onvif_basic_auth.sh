#!/bin/sh

# Test ONVIF with HTTP Basic Authentication
# Username: thingino, Password: thingino (base64: dGhpbmdpbm86dGhpbmdpbm8=)

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
export HTTP_AUTHORIZATION="Basic dGhpbmdpbm86dGhpbmdpbm8="

# Simple ONVIF GetCapabilities request (no WS-Security)
SOAP_REQUEST='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetCapabilities>
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Calculate content length
export CONTENT_LENGTH=${#SOAP_REQUEST}

echo "Testing ONVIF with HTTP Basic Authentication..."
echo "Authorization: Basic dGhpbmdpbm86dGhpbmdpbm8= (thingino:thingino)"
echo "Content Length: $CONTENT_LENGTH"
echo ""
echo "Response:"
echo "----------------------------------------"

# Send the request
echo "$SOAP_REQUEST" | /mnt/nfs/onvif_simple_server -c /etc/onvif.json -d 5

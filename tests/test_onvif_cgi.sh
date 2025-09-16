#!/bin/sh

# Test script for ONVIF Simple Server CGI emulation
# This script sets up the proper CGI environment variables and sends a test SOAP request

# Set CGI environment variables
export REQUEST_METHOD="POST"
export CONTENT_TYPE="application/soap+xml; charset=utf-8"
export SERVER_NAME="192.168.88.201"
export SERVER_PORT="80"
export SCRIPT_NAME="/onvif/device_service"
export REQUEST_URI="/onvif/device_service"
export SERVER_PROTOCOL="HTTP/1.1"
export GATEWAY_INTERFACE="CGI/1.1"
export SERVER_SOFTWARE="nginx/1.18.0"

# Sample ONVIF GetCapabilities request with WS-Security authentication
# Username: thingino, Password: thingino (from your config)
# Using PasswordDigest format as expected by the server
SOAP_REQUEST='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand="true">
            <wsse:UsernameToken wsu:Id="UsernameToken-1">
                <wsse:Username>thingino</wsse:Username>
                <wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">/N95JlwSNNDNtjnSrFhN5272+d0=</wsse:Password>
                <wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">LKqI6G/AikKCQrN0zqZFlg==</wsse:Nonce>
                <wsu:Created>2024-01-01T00:00:00Z</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        <tds:GetCapabilities>
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

# Calculate content length
export CONTENT_LENGTH=${#SOAP_REQUEST}

echo "Testing ONVIF Simple Server with CGI environment..."
echo "Request Method: $REQUEST_METHOD"
echo "Content Type: $CONTENT_TYPE"
echo "Content Length: $CONTENT_LENGTH"
echo "Server: $SERVER_NAME:$SERVER_PORT"
echo ""
echo "SOAP Request:"
echo "$SOAP_REQUEST"
echo ""
echo "Response:"
echo "----------------------------------------"

# Send the request to the ONVIF server
echo "$SOAP_REQUEST" | /mnt/nfs/onvif_simple_server -c /etc/onvif.json -d 5

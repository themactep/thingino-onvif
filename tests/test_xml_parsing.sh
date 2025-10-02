#!/bin/bash

# Test XML parsing with the problematic request from the logs
# This reproduces the issue where v:Header is found instead of v:Envelope

# The XML from the logs (reconstructed from the first 200 chars shown)
# Using a namespace prefix "v:" which seems to be causing issues
SOAP_REQUEST='<?xml version="1.0" encoding="UTF-8"?>
<v:Envelope xmlns:i="http://www.w3.org/2001/XMLSchema-instance" xmlns:d="http://www.w3.org/2001/XMLSchema" xmlns:c="http://www.w3.org/2003/05/soap-envelope" xmlns:v="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <v:Header/>
    <v:Body>
        <tds:GetCapabilities>
            <tds:Category>All</tds:Category>
        </tds:GetCapabilities>
    </v:Body>
</v:Envelope>'

echo "Testing XML parsing with v: namespace prefix..."
echo "XML Request:"
echo "$SOAP_REQUEST"
echo ""
echo "Response:"
echo "----------------------------------------"

# Calculate content length
export CONTENT_LENGTH=${#SOAP_REQUEST}
export REQUEST_METHOD=POST
export CONTENT_TYPE="application/soap+xml; charset=utf-8"
export SERVER_NAME="192.168.1.100"
export SERVER_PORT="80"

# Send the request
echo "$SOAP_REQUEST" | ./onvif_simple_server -c /etc/onvif.json -d 5 2>&1

echo ""
echo "Test completed."


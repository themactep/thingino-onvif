#!/bin/bash

# Simple one-liner test for ONVIF GetCapabilities
# This is the minimal command to test ONVIF functionality

echo '<?xml version="1.0" encoding="UTF-8"?><SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><SOAP-ENV:Header/><SOAP-ENV:Body><tds:GetCapabilities><tds:Category>All</tds:Category></tds:GetCapabilities></SOAP-ENV:Body></SOAP-ENV:Envelope>' | \
REQUEST_METHOD=POST \
CONTENT_TYPE="application/soap+xml; charset=utf-8" \
CONTENT_LENGTH=295 \
SERVER_NAME="192.168.1.100" \
SERVER_PORT="80" \
/mnt/nfs/onvif_simple_server -c /etc/onvif.json -d 5

#!/bin/sh

# Test simpler ONVIF methods that are more likely to work

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

test_method() {
    local method_name="$1"
    local soap_body="$2"

    echo "========================================="
    echo "Testing: $method_name"
    echo "========================================="

    # Create SOAP request with authentication
    SOAP_REQUEST="<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand=\"true\">
            <wsse:UsernameToken wsu:Id=\"UsernameToken-1\">
                <wsse:Username>thingino</wsse:Username>
                <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">/N95JlwSNNDNtjnSrFhN5272+d0=</wsse:Password>
                <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">LKqI6G/AikKCQrN0zqZFlg==</wsse:Nonce>
                <wsu:Created>2024-01-01T00:00:00Z</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        $soap_body
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>"

    export CONTENT_LENGTH=${#SOAP_REQUEST}
    echo "Content-Length: $CONTENT_LENGTH"
    echo ""
    echo "Response:"
    echo "-----------------------------------------"

    echo "$SOAP_REQUEST" | /var/www/onvif/device_service -c /etc/onvif.json -d 5
    echo ""
    echo ""
}

echo "Testing ONVIF Simple Methods with Authentication..."
echo ""

# Test 1: GetDeviceInformation (usually works)
test_method "GetDeviceInformation" "<tds:GetDeviceInformation/>"

# Test 2: GetSystemDateAndTime (usually works)
test_method "GetSystemDateAndTime" "<tds:GetSystemDateAndTime/>"

# Test 3: GetServices (usually works)
test_method "GetServices" "<tds:GetServices><tds:IncludeCapability>false</tds:IncludeCapability></tds:GetServices>"

# Test 4: GetCapabilities with Device category only
test_method "GetCapabilities-Device" "<tds:GetCapabilities><tds:Category>Device</tds:Category></tds:GetCapabilities>"

echo "Test completed!"
echo ""
echo "Check /tmp/onvif_raw.log for detailed request/response logs:"
echo "cat /tmp/onvif_raw.log"

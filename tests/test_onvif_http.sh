#!/bin/sh

# Test ONVIF server through HTTP (localhost) instead of direct CGI execution

echo "Testing ONVIF Server through HTTP (localhost)..."
echo "================================================"

# Clear previous raw log
> /tmp/onvif_raw.log

test_http_method() {
    local method_name="$1"
    local soap_body="$2"
    local url="$3"
    
    echo ""
    echo "========================================="
    echo "Testing: $method_name via HTTP"
    echo "URL: $url"
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
    
    echo "Content-Length: ${#SOAP_REQUEST}"
    echo ""
    echo "HTTP Response:"
    echo "-----------------------------------------"
    
    # Make HTTP request with curl
    curl -v \
        -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -H "Content-Length: ${#SOAP_REQUEST}" \
        -H "SOAPAction: \"\"" \
        --data "$SOAP_REQUEST" \
        "$url" 2>&1
    
    echo ""
    echo ""
}

# Test different URLs and methods
echo "1. Testing GetDeviceInformation..."
test_http_method "GetDeviceInformation" "<tds:GetDeviceInformation/>" "http://localhost/onvif/device_service"

echo "2. Testing GetSystemDateAndTime..."
test_http_method "GetSystemDateAndTime" "<tds:GetSystemDateAndTime/>" "http://localhost/onvif/device_service"

echo "3. Testing GetServices..."
test_http_method "GetServices" "<tds:GetServices><tds:IncludeCapability>false</tds:IncludeCapability></tds:GetServices>" "http://localhost/onvif/device_service"

echo "4. Testing GetCapabilities..."
test_http_method "GetCapabilities" "<tds:GetCapabilities><tds:Category>Device</tds:Category></tds:GetCapabilities>" "http://localhost/onvif/device_service"

echo ""
echo "Test completed!"
echo ""
echo "Check /tmp/onvif_raw.log for detailed request/response logs:"
echo "cat /tmp/onvif_raw.log"

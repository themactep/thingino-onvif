#!/bin/bash

# ONVIF Simple Server Container Test Script
# This script tests the ONVIF server functionality through HTTP requests

CONTAINER_NAME="oss"
SERVER_URL="http://localhost:8000"
VERBOSE=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [-v|--verbose] [-h|--help]"
            echo ""
            echo "Options:"
            echo "  -v, --verbose    Show raw XML responses for all tests"
            echo "  -h, --help       Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}=== ONVIF Simple Server Container Test Suite ===${NC}"
if [ "$VERBOSE" = true ]; then
    echo -e "${YELLOW}Verbose mode enabled - showing raw XML responses${NC}"
fi

# Check if container is running
if ! podman ps | grep -q $CONTAINER_NAME; then
    echo -e "${RED}Error: Container '$CONTAINER_NAME' is not running${NC}"
    echo "Start it with: ./container_run.sh"
    exit 1
fi

echo -e "${GREEN}✓ Container '$CONTAINER_NAME' is running${NC}"

# Function to generate authenticated SOAP request
generate_auth_soap() {
    local method=$1
    local username="thingino"
    local password="thingino"

    # Use Python script if available, otherwise skip auth
    if [ -f "tests/generate_onvif_auth.py" ]; then
        python3 -c "
import sys
sys.path.insert(0, 'tests')
from generate_onvif_auth import generate_soap_request
soap = generate_soap_request('$username', '$password', '$method')
# Remove Category element if not needed
if '$method' == 'GetVideoSources':
    soap = soap.replace('<tds:Category>All</tds:Category>', '')
print(soap)
"
    else
        echo ""
    fi
}

# Function to test ONVIF service
test_onvif_service() {
    local service_name=$1
    local method=$2
    local description=$3
    local require_auth=${4:-false}
    local expected_contains=${5:-}

    echo -e "\n${YELLOW}Testing: $description${NC}"

    # Create SOAP request
    if [ "$require_auth" = true ]; then
        # Generate authenticated request
        local soap_request=$(generate_auth_soap "$method")
        if [ -z "$soap_request" ]; then
            echo -e "${YELLOW}⚠ Skipping $method - authentication script not available${NC}"
            return 0
        fi
    else
        # Create simple SOAP request (using SOAP 1.2 format that ONVIF expects)
        local soap_request="<?xml version=\"1.0\" encoding=\"utf-8\"?>
<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"
            xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">
  <s:Header/>
  <s:Body>
    <tds:$method/>
  </s:Body>
</s:Envelope>"
    fi

    # Send request
    local response=$(curl -s -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -H "SOAPAction: \"http://www.onvif.org/ver10/device/wsdl/$method\"" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/$service_name")

    # Check if we got a valid SOAP response
    if [ $? -eq 0 ] && [[ $response == *"<?xml"* ]] && \
       [[ $response == *"Envelope"* ]] && \
       [[ $response == *"Body"* ]]; then

        # Check if it's a SOAP Fault
        if [[ $response == *"Fault"* ]]; then
            # Extract fault reason if possible
            if [[ $response =~ Text.*\>([^<]+)\<.*Text ]]; then
                fault_reason="${BASH_REMATCH[1]}"
                echo -e "${YELLOW}⚠ $service_name/$method - SOAP FAULT: $fault_reason${NC}"
            else
                echo -e "${YELLOW}⚠ $service_name/$method - SOAP FAULT${NC}"
            fi
            if [ "$VERBOSE" = true ]; then
                echo "Response: $response"
            fi
            return 2  # Return 2 for SOAP fault (different from success or error)
        else
            # Optional content assertion
            if [ -n "$expected_contains" ]; then
                if [[ $response == *"$expected_contains"* ]]; then
                    echo -e "${GREEN}✓ $service_name/$method - SUCCESS (asserted: '$expected_contains')${NC}"
                else
                    echo -e "${RED}✗ $service_name/$method - FAILED (missing expected content)${NC}"
                    echo "Expected to find: $expected_contains"
                    if [ "$VERBOSE" = true ]; then
                        echo "Response: $response"
                    fi
                    return 1
                fi
            else
                echo -e "${GREEN}✓ $service_name/$method - SUCCESS${NC}"
            fi
            if [ "$VERBOSE" = true ]; then
                echo "Response: $response"
            fi
            return 0
        fi
    elif [ $? -ne 0 ]; then
        echo -e "${RED}✗ $service_name/$method - FAILED (curl error)${NC}"
        return 1
    elif [[ -z "$response" ]]; then
        echo -e "${RED}✗ $service_name/$method - FAILED (no response)${NC}"
        return 1
    else
        echo -e "${RED}✗ $service_name/$method - FAILED (invalid response)${NC}"
        echo "Response: $response"
        return 1
    fi
}

# Function to test PTZ service with custom SOAP body
test_ptz_service() {
    local method=$1
    local description=$2
    local soap_body=$3
    local expected_http_code=${4:-200}
    local expected_fault=${5:-""}

    echo -e "\n${YELLOW}Testing: $description${NC}"

    local username="thingino"
    local password="thingino"

    # Generate authenticated SOAP request with custom body
    local soap_request=$(python3 -c "
import sys
import base64
import hashlib
import datetime
import os

username = '$username'
password = '$password'
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')

soap = '''<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\"
                   xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\"
                   xmlns:tt=\"http://www.onvif.org/ver10/schema\"
                   xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\"
                   xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand=\"true\">
            <wsse:UsernameToken wsu:Id=\"UsernameToken-1\">
                <wsse:Username>{username}</wsse:Username>
                <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">{digest}</wsse:Password>
                <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">{nonce}</wsse:Nonce>
                <wsu:Created>{created}</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        $soap_body
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'''.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created)
print(soap)
")

    # Send request and capture both response and HTTP code
    local http_code=$(curl -s -w "%{http_code}" -o /tmp/ptz_response.xml -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/ptz_service")

    local response=$(cat /tmp/ptz_response.xml)

    # Check HTTP status code
    if [ "$http_code" != "$expected_http_code" ]; then
        echo -e "${RED}✗ $method - FAILED (expected HTTP $expected_http_code, got $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "Response: $response"
        fi
        return 1
    fi

    # Check if we got a valid SOAP response
    if [[ $response == *"<?xml"* ]] && [[ $response == *"Envelope"* ]] && [[ $response == *"Body"* ]]; then

        # If we expect a fault, check for it
        if [ -n "$expected_fault" ]; then
            if [[ $response == *"$expected_fault"* ]]; then
                echo -e "${GREEN}✓ $method - SUCCESS (HTTP $http_code with expected fault: $expected_fault)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 0
            else
                echo -e "${RED}✗ $method - FAILED (expected fault '$expected_fault' not found)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 1
            fi
        else
            # No fault expected, check for success response
            if [[ $response == *"Fault"* ]]; then
                echo -e "${RED}✗ $method - FAILED (unexpected SOAP fault)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 1
            else
                echo -e "${GREEN}✓ $method - SUCCESS (HTTP $http_code)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "Response: $response"
                fi
                return 0
            fi
        fi
    else
        echo -e "${RED}✗ $method - FAILED (invalid SOAP response)${NC}"
        echo "Response: $response"
        return 1
    fi
}
# Explicit PRE_AUTH verification helpers (check HTTP status codes)
preauth_post() {
    local service_path=$1
    local body=$2
    local out=$3
    curl -s -w "%{http_code}" -o "$out" -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$body" \
        "$SERVER_URL/$service_path"
}

test_preauth_device_http() {
    local method=$1
    local description=$2
    local expected_http_code=$3

    echo -e "\n${YELLOW}PRE_AUTH: $description${NC}"

    local soap_request="<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">\n  <s:Header/>\n  <s:Body>\n    <tds:$method/>\n  </s:Body>\n</s:Envelope>"

    local code=$(preauth_post "onvif/device_service" "$soap_request" "/tmp/preauth_${method}.xml")
    if [ "$code" = "$expected_http_code" ]; then
        echo -e "${GREEN}✓ $method - HTTP $code as expected${NC}"
        return 0
    else
        echo -e "${RED}✗ $method - expected HTTP $expected_http_code, got $code${NC}"
        [ -f "/tmp/preauth_${method}.xml" ] && echo "Response: $(head -c 200 /tmp/preauth_${method}.xml) ..."
        return 1
    fi
}

test_preauth_events_http() {
    local method=$1
    local description=$2
    local expected_http_code=$3

    echo -e "\n${YELLOW}PRE_AUTH: $description${NC}"

    local soap_request="<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:tev=\"http://www.onvif.org/ver10/events/wsdl\">\n  <s:Header/>\n  <s:Body>\n    <tev:$method/>\n  </s:Body>\n</s:Envelope>"

    local code=$(preauth_post "onvif/events_service" "$soap_request" "/tmp/preauth_tev_${method}.xml")
    if [ "$code" = "$expected_http_code" ]; then
        echo -e "${GREEN}✓ tev:$method - HTTP $code as expected${NC}"
        return 0
    else
        echo -e "${RED}✗ tev:$method - expected HTTP $expected_http_code, got $code${NC}"
        [ -f "/tmp/preauth_tev_${method}.xml" ] && echo "Response: $(head -c 200 /tmp/preauth_tev_${method}.xml) ..."
        return 1
    fi
}




# Function to test basic HTTP connectivity
test_http_connectivity() {
    echo -e "\n${YELLOW}Testing HTTP connectivity...${NC}"

    local response=$(curl -s -o /dev/null -w "%{http_code}" "$SERVER_URL/")

    # Accept any HTTP response code that indicates the server is running
    # 200 = OK, 403 = Forbidden (normal for directory without index), 404 = Not Found
    if [ "$response" = "200" ] || [ "$response" = "403" ] || [ "$response" = "404" ]; then
        echo -e "${GREEN}✓ HTTP server is responding (code: $response)${NC}"
        return 0
    else
        echo -e "${RED}✗ HTTP server not responding (code: $response)${NC}"
        return 1
    fi
}

# Function to test CGI execution
test_cgi_execution() {
    echo -e "\n${YELLOW}Testing CGI execution...${NC}"

    # Test with GET request to check if CGI scripts are executing
    local response=$(curl -s "$SERVER_URL/onvif/device_service")

    # Check if we get a valid response indicating CGI execution
    if [[ $response == *"HTTP method not supported"* ]] || \
       [[ $response == *"<?xml"* ]] || \
       [[ $response == *"<soap:"* ]] || \
       [[ $response == *"<SOAP-ENV:"* ]]; then
        echo -e "${GREEN}✓ CGI scripts are executing${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "Response: $response"
        fi
        return 0
    elif [[ -z "$response" ]]; then
        echo -e "${RED}✗ CGI execution failed - no response${NC}"
        return 1
    else
        echo -e "${RED}✗ CGI execution failed - unexpected response${NC}"
        echo "Response: $response"
        return 1
    fi
}

# Run tests
echo -e "\n${BLUE}=== Running Tests ===${NC}"

test_http_connectivity
test_cgi_execution


# PRE_AUTH verification (must be accessible without auth)
echo -e "\n${BLUE}=== PRE_AUTH Verification ===${NC}"
# Expected 200 without auth
test_preauth_device_http "GetSystemDateAndTime" "Device:GetSystemDateAndTime should be pre-auth" 200
test_preauth_device_http "GetWsdlUrl"         "Device:GetWsdlUrl should be pre-auth" 200
test_preauth_device_http "GetHostname"        "Device:GetHostname should be pre-auth" 200
test_preauth_device_http "GetEndpointReference" "Device:GetEndpointReference should be pre-auth" 200
test_preauth_device_http "GetServices"        "Device:GetServices should be pre-auth" 200
test_preauth_device_http "GetServiceCapabilities" "Device:GetServiceCapabilities should be pre-auth" 200
test_preauth_device_http "GetCapabilities"     "Device:GetCapabilities should be pre-auth" 200
# Events service pre-auth
test_preauth_events_http "GetServiceCapabilities" "Events:GetServiceCapabilities should be pre-auth" 200

# Expected 401 without auth (not in PRE_AUTH)
# Note: these are implemented; they should now require auth when username is set
if test_preauth_device_http "GetDeviceInformation" "Device:GetDeviceInformation should require auth" 401; then
  echo -e "${GREEN}\u2713 Device:GetDeviceInformation requires auth (HTTP 401)${NC}"
fi
if test_preauth_device_http "GetUsers" "Device:GetUsers should require auth" 401; then
  echo -e "${GREEN}\u2713 Device:GetUsers requires auth (HTTP 401)${NC}"
fi

# Test basic ONVIF device services (no auth required)
test_onvif_service "device_service" "GetSystemDateAndTime" "Get System Date and Time" false
# Requires auth: expect SOAP fault
test_onvif_service "device_service" "GetDeviceInformation" "Get Device Information" false
# General capability retrieval
test_onvif_service "device_service" "GetCapabilities" "Get Device Capabilities" false
# Assert DHCP flag now defaults to true per spec convenience
test_onvif_service "device_service" "GetHostname" "Get Hostname" false "<tt:FromDHCP>true</tt:FromDHCP>"
# Assert EPR includes an Address element with HTTP URL
test_onvif_service "device_service" "GetEndpointReference" "Get EndpointReference" false "<wsa5:Address>http://"
# Basic check for services
test_onvif_service "device_service" "GetServices" "Get Available Services" false

# Test media services (auth required)
test_onvif_service "media_service" "GetProfiles" "Get Media Profiles" true
test_onvif_service "media_service" "GetVideoSources" "Get Video Sources" true

# PTZ Tests
echo -e "\n${BLUE}=== PTZ Functionality Tests ===${NC}"

# Test PTZ GetConfigurations
test_ptz_get_configurations() {
    echo -e "\n${YELLOW}Testing: PTZ GetConfigurations (verify UseCount)${NC}"

    local soap_request=$(generate_auth_soap "GetConfigurations")
    if [ -z "$soap_request" ]; then


        echo -e "${YELLOW}⚠ Skipping PTZ tests - authentication script not available${NC}"
        return 0
    fi

    local response=$(curl -s -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/ptz_service")

    if [[ $response == *"PTZConfiguration"* ]] && [[ $response == *"UseCount"* ]]; then
        # Extract UseCount value
        if [[ $response =~ \<tt:UseCount\>([0-9]+)\</tt:UseCount\> ]]; then
            local use_count="${BASH_REMATCH[1]}"
            if [ "$use_count" -gt 0 ]; then
                echo -e "${GREEN}✓ PTZ GetConfigurations - SUCCESS (UseCount=$use_count)${NC}"
                if [ "$VERBOSE" = true ]; then
                    echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
                fi
                return 0
            else
                echo -e "${YELLOW}⚠ PTZ GetConfigurations - UseCount is 0 (should be >0)${NC}"
                return 2
            fi
        else
            echo -e "${YELLOW}⚠ PTZ GetConfigurations - Could not extract UseCount${NC}"
            return 2
        fi
    else
        echo -e "${RED}✗ PTZ GetConfigurations - FAILED${NC}"
        return 1
    fi
}

# Test PTZ GetNodes
test_ptz_get_nodes() {
    echo -e "\n${YELLOW}Testing: PTZ GetNodes (verify capabilities)${NC}"

    local soap_request=$(generate_auth_soap "GetNodes")
    if [ -z "$soap_request" ]; then
        return 0
    fi

    local response=$(curl -s -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/ptz_service")

    if [[ $response == *"PTZNode"* ]] && [[ $response == *"AbsolutePanTiltPositionSpace"* ]]; then
        echo -e "${GREEN}✓ PTZ GetNodes - SUCCESS (AbsoluteMove supported)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ GetNodes - FAILED${NC}"
        return 1
    fi
}

# Test PTZ AbsoluteMove with valid coordinates
test_ptz_absolute_move_valid() {
    echo -e "\n${YELLOW}Testing: PTZ AbsoluteMove with valid coordinates${NC}"

    local username="thingino"
    local password="thingino"

    # Generate authenticated AbsoluteMove request
    local soap_request=$(python3 -c "
import sys, base64, hashlib, datetime, os
sys.path.insert(0, 'tests')

username = '$username'
password = '$password'
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')

soap = '''<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\"
                   xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\"
                   xmlns:tt=\"http://www.onvif.org/ver10/schema\"
                   xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\"
                   xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand=\"true\">
            <wsse:UsernameToken wsu:Id=\"UsernameToken-1\">
                <wsse:Username>{username}</wsse:Username>
                <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">{digest}</wsse:Password>
                <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">{nonce}</wsse:Nonce>
                <wsu:Created>{created}</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        <tptz:AbsoluteMove>
            <tptz:ProfileToken>Profile_0</tptz:ProfileToken>
            <tptz:Position>
                <tt:PanTilt x=\"1850.0\" y=\"500.0\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\"/>
            </tptz:Position>
        </tptz:AbsoluteMove>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'''.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created)
print(soap)
" 2>/dev/null)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    local http_code=$(curl -s -o /tmp/ptz_response.xml -w "%{http_code}" -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/ptz_service")

    local response=$(cat /tmp/ptz_response.xml)

    if [ "$http_code" = "200" ] && [[ $response == *"AbsoluteMoveResponse"* ]]; then
        echo -e "${GREEN}✓ PTZ AbsoluteMove (valid) - SUCCESS (HTTP $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ AbsoluteMove (valid) - FAILED (HTTP $http_code)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response"
        fi
        return 1
    fi
}

# Test PTZ AbsoluteMove with out-of-bounds coordinates (should return HTTP 500)
test_ptz_absolute_move_invalid() {
    echo -e "\n${YELLOW}Testing: PTZ AbsoluteMove with out-of-bounds coordinates (expect HTTP 500)${NC}"

    local username="thingino"
    local password="thingino"

    # Generate authenticated AbsoluteMove request with invalid coordinates
    local soap_request=$(python3 -c "
import sys, base64, hashlib, datetime, os
sys.path.insert(0, 'tests')

username = '$username'
password = '$password'
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')

soap = '''<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\"
                   xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\"
                   xmlns:tt=\"http://www.onvif.org/ver10/schema\"
                   xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\"
                   xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand=\"true\">
            <wsse:UsernameToken wsu:Id=\"UsernameToken-1\">
                <wsse:Username>{username}</wsse:Username>
                <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">{digest}</wsse:Password>
                <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">{nonce}</wsse:Nonce>
                <wsu:Created>{created}</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        <tptz:AbsoluteMove>
            <tptz:ProfileToken>Profile_0</tptz:ProfileToken>
            <tptz:Position>
                <tt:PanTilt x=\"9999.0\" y=\"9999.0\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/PositionGenericSpace\"/>
            </tptz:Position>
        </tptz:AbsoluteMove>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'''.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created)
print(soap)
" 2>/dev/null)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    local http_code=$(curl -s -o /tmp/ptz_fault_response.xml -w "%{http_code}" -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/ptz_service")

    local response=$(cat /tmp/ptz_fault_response.xml)

    if [ "$http_code" = "500" ] && [[ $response == *"Fault"* ]] && [[ $response == *"InvalidPosition"* ]]; then
        echo -e "${GREEN}✓ PTZ AbsoluteMove (invalid) - SUCCESS (HTTP $http_code with ter:InvalidPosition fault)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ AbsoluteMove (invalid) - FAILED (HTTP $http_code, expected 500)${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response"
        fi
        return 1
    fi
}

# Test PTZ GetStatus
test_ptz_get_status() {
    echo -e "\n${YELLOW}Testing: PTZ GetStatus${NC}"

    local username="thingino"
    local password="thingino"

    # Generate authenticated GetStatus request
    local soap_request=$(python3 -c "
import sys, base64, hashlib, datetime, os
sys.path.insert(0, 'tests')

username = '$username'
password = '$password'
nonce = os.urandom(16)
nonce_b64 = base64.b64encode(nonce).decode('utf-8')
created = datetime.datetime.now(datetime.UTC).strftime('%Y-%m-%dT%H:%M:%S+00:00')
digest_input = nonce + created.encode('utf-8') + password.encode('utf-8')
digest = hashlib.sha1(digest_input).digest()
digest_b64 = base64.b64encode(digest).decode('utf-8')

soap = '''<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\"
                   xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\"
                   xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\"
                   xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">
    <SOAP-ENV:Header>
        <wsse:Security SOAP-ENV:mustUnderstand=\"true\">
            <wsse:UsernameToken wsu:Id=\"UsernameToken-1\">
                <wsse:Username>{username}</wsse:Username>
                <wsse:Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">{digest}</wsse:Password>
                <wsse:Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">{nonce}</wsse:Nonce>
                <wsu:Created>{created}</wsu:Created>
            </wsse:UsernameToken>
        </wsse:Security>
    </SOAP-ENV:Header>
    <SOAP-ENV:Body>
        <tptz:GetStatus>
            <tptz:ProfileToken>Profile_0</tptz:ProfileToken>
        </tptz:GetStatus>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'''.format(username=username, digest=digest_b64, nonce=nonce_b64, created=created)
print(soap)
" 2>/dev/null)

    if [ -z "$soap_request" ]; then
        echo -e "${YELLOW}⚠ Skipping - Python not available${NC}"
        return 0
    fi

    local response=$(curl -s -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/ptz_service")

    if [[ $response == *"GetStatusResponse"* ]] && [[ $response == *"Position"* ]] && [[ $response == *"PanTilt"* ]]; then
        echo -e "${GREEN}✓ PTZ GetStatus - SUCCESS${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        fi
        return 0
    else
        echo -e "${RED}✗ PTZ GetStatus - FAILED${NC}"
        return 1
    fi
}

# Run PTZ tests
test_ptz_get_configurations
test_ptz_get_nodes
test_ptz_absolute_move_valid
test_ptz_absolute_move_invalid
test_ptz_get_status


# Events (PullPoint) Tests
if bash tests/container_test_events.sh; then
  echo -e "${GREEN}✓ Events tests - SUCCESS${NC}"
else
  echo -e "${RED}✗ Events tests - FAILED${NC}"
fi

echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "${GREEN}Tests completed. Check output above for results.${NC}"
echo -e "\n${YELLOW}To view container logs:${NC} podman logs $CONTAINER_NAME"
echo -e "${YELLOW}To inspect container:${NC} ./container_inspect.sh"

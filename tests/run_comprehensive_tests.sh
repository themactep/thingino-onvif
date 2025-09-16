#!/bin/bash

# Comprehensive ONVIF Simple Server Test Suite
# This script runs extensive ONVIF SOAP requests to test all server functionality

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQUESTS_DIR="$SCRIPT_DIR/requests"
#SERVER_URL="http://localhost:8000/onvif"
SERVER_URL="http://192.168.88.201:80/onvif"
USERNAME="thingino"
PASSWORD="thingino"

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Debug mode - set to true to enable detailed HTTP communication logging
DEBUG_MODE="${DEBUG_MODE:-true}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 Comprehensive ONVIF Test Suite${NC}"
echo "===================================="
echo -e "${CYAN}Enhanced with detailed HTTP communication logging${NC}"
if [[ "$DEBUG_MODE" == "true" ]]; then
    echo -e "${GREEN}🔍 Debug Mode: ENABLED - Full request/response logging${NC}"
else
    echo -e "${YELLOW}🔍 Debug Mode: DISABLED - Set DEBUG_MODE=true for full logging${NC}"
fi
echo ""

# Function to add authentication to SOAP request
add_authentication() {
    local xml_file="$1"
    local temp_file=$(mktemp)

    # Check if this request needs authentication (not in the exempt list)
    local basename=$(basename "$xml_file")
    case "$basename" in
        "test_getcapabilities.xml"|"test_getservices.xml"|"device_getsystemdateandtime.xml"|"device_getusers.xml")
            # These don't need authentication, just copy the file
            cp "$xml_file" "$temp_file"
            ;;
        *)
            # Add WS-Security header to requests that need authentication
            local username="$USERNAME"
            local password="$PASSWORD"

            # Generate nonce and timestamp
            local nonce=$(openssl rand -base64 16)
            local timestamp=$(date -u +"%Y-%m-%dT%H:%M:%S.%3NZ")

            # Calculate password digest using Python to handle binary data properly
            local digest=$(python3 -c "
import base64
import hashlib

nonce = '$nonce'
timestamp = '$timestamp'
password = '$password'

# Decode nonce from base64
nonce_bytes = base64.b64decode(nonce)

# Combine nonce + timestamp + password
combined = nonce_bytes + timestamp.encode('utf-8') + password.encode('utf-8')

# Calculate SHA1 hash and encode as base64
digest = base64.b64encode(hashlib.sha1(combined).digest()).decode('utf-8')
print(digest)
")
            # Create authenticated SOAP request using Python to avoid sed issues
            python3 -c "
import sys
import xml.etree.ElementTree as ET

# Read the original XML
with open('$xml_file', 'r') as f:
    content = f.read()

# Parse and modify
root = ET.fromstring(content)

# Find the Header element
header = root.find('.//{http://www.w3.org/2003/05/soap-envelope}Header')
if header is not None:
    # Create Security element
    security = ET.SubElement(header, '{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd}Security')
    security.set('{http://www.w3.org/2003/05/soap-envelope}mustUnderstand', 'true')

    # Create UsernameToken
    token = ET.SubElement(security, '{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd}UsernameToken')
    token.set('{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd}Id', 'UsernameToken-1')

    # Add username
    username_elem = ET.SubElement(token, '{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd}Username')
    username_elem.text = '$username'

    # Add password
    password_elem = ET.SubElement(token, '{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd}Password')
    password_elem.set('Type', 'http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest')
    password_elem.text = '$digest'

    # Add nonce
    nonce_elem = ET.SubElement(token, '{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd}Nonce')
    nonce_elem.set('EncodingType', 'http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary')
    nonce_elem.text = '$nonce'

    # Add timestamp
    created_elem = ET.SubElement(token, '{http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd}Created')
    created_elem.text = '$timestamp'

# Write the modified XML
with open('$temp_file', 'w') as f:
    f.write('<?xml version=\"1.0\" encoding=\"UTF-8\"?>')
    f.write(ET.tostring(root, encoding='unicode'))
"
            ;;
    esac

    echo "$temp_file"
}

# Function to run a SOAP test
run_soap_test() {
    local test_name="$1"
    local service="$2"
    local xml_file="$3"
    local expected_pattern="$4"
    local optional="${5:-false}"

    echo -e "${CYAN}Testing: $test_name${NC}"
    echo "Service: $service"
    echo "Request: $(basename "$xml_file")"

    if [ ! -f "$xml_file" ]; then
        echo -e "${RED}❌ FAIL: Test file not found: $xml_file${NC}"
        ((TESTS_FAILED++))
        echo ""
        return 1
    fi

    # Add authentication if needed
    local auth_xml_file=$(add_authentication "$xml_file")

    # Display request URL
    local request_url="$SERVER_URL/$service"
    echo -e "${BLUE}🌐 Request URL:${NC} $request_url"

    # Display raw XML request (if debug mode enabled)
    if [[ "$DEBUG_MODE" == "true" ]]; then
        echo -e "${BLUE}📤 Raw XML Request:${NC}"
        echo "----------------------------------------"
        cat "$auth_xml_file" | xmllint --format - 2>/dev/null || cat "$auth_xml_file"
        echo "----------------------------------------"
    fi

    # Make the SOAP request
    local response
    local http_code

    cmd="curl -s -w \"%{http_code}\" -X POST "
    cmd="$cmd -H 'Content-Type: application/soap+xml'"
    cmd="$cmd -H 'SOAPAction: \"\"'"
    cmd="$cmd -d @\"$auth_xml_file\""
    cmd="$cmd $request_url"

    echo -e "${BLUE}🔧 cURL Command:${NC} $cmd"

    response=$($cmd 2>/dev/null || echo "ERROR000")

    # Clean up temporary file
    rm -f "$auth_xml_file"

    # Extract HTTP status code
    http_code="${response: -3}"
    response="${response%???}"

    # Display HTTP status code
    echo -e "${BLUE}📊 HTTP Status Code:${NC} $http_code"

    # Display raw XML response (if debug mode enabled)
    if [[ "$DEBUG_MODE" == "true" ]]; then
        echo -e "${BLUE}📥 Raw XML Response:${NC}"
        echo "========================================"
        if [[ -n "$response" ]] && [[ "$response" != "ERROR" ]]; then
            echo "$response" | xmllint --format - 2>/dev/null || echo "$response"
        else
            echo -e "${RED}No response received or connection error${NC}"
        fi
        echo "========================================"
    fi

    if [[ "$response" == "ERROR" ]] || [[ -z "$response" ]] || [[ "$http_code" != "200" ]]; then
        if [[ "$optional" == "true" ]]; then
            echo -e "${YELLOW}⚠️  SKIP: Optional test failed (HTTP: $http_code)${NC}"
            ((TESTS_SKIPPED++))
        else
            echo -e "${RED}❌ FAIL: No response or error from server (HTTP: $http_code)${NC}"
            ((TESTS_FAILED++))
        fi
        echo ""
        return 1
    fi

    # Check if response contains expected pattern
    echo -e "${BLUE}🔍 Test Evaluation:${NC}"
    if [[ -n "$expected_pattern" ]]; then
        echo "Expected pattern: $expected_pattern"
        if [[ "$response" =~ $expected_pattern ]]; then
            echo -e "${GREEN}✅ PASS: Response contains expected pattern${NC}"
            ((TESTS_PASSED++))
        else
            if [[ "$optional" == "true" ]]; then
                echo -e "${YELLOW}⚠️  SKIP: Optional test - pattern not found${NC}"
                ((TESTS_SKIPPED++))
            else
                echo -e "${RED}❌ FAIL: Response does not contain expected pattern${NC}"
                ((TESTS_FAILED++))
            fi
            echo ""
            return 1
        fi
    else
        echo -e "${GREEN}✅ PASS: Got valid response (no pattern check required)${NC}"
        ((TESTS_PASSED++))
    fi

    # Show key parts of response for debugging
    echo -e "${BLUE}📋 Response Highlights:${NC}"
    show_response_highlights "$response"

    echo ""
    echo "=================================================="
    echo ""
    return 0
}

# Function to show interesting parts of the response
show_response_highlights() {
    local response="$1"

    # Extract and show various ONVIF elements
    if [[ "$response" =~ \<tt:RelayOutputs\>([^<]*)\</tt:RelayOutputs\> ]]; then
        echo -e "📊 RelayOutputs: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:VideoSources\>([^<]*)\</tt:VideoSources\> ]]; then
        echo -e "📊 VideoSources: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:AudioSources\>([^<]*)\</tt:AudioSources\> ]]; then
        echo -e "📊 AudioSources: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:Manufacturer\>([^<]*)\</tt:Manufacturer\> ]]; then
        echo -e "📊 Manufacturer: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:Model\>([^<]*)\</tt:Model\> ]]; then
        echo -e "📊 Model: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:SerialNumber\>([^<]*)\</tt:SerialNumber\> ]]; then
        echo -e "📊 SerialNumber: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:FirmwareVersion\>([^<]*)\</tt:FirmwareVersion\> ]]; then
        echo -e "📊 FirmwareVersion: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi
}

# Function to check if server is running
check_server() {
    echo -e "${BLUE}🔍 Checking if test server is running...${NC}"
    if curl -s "$SERVER_URL" > /dev/null 2>&1; then
        echo -e "${GREEN}✅ Server is running at $SERVER_URL${NC}"
        echo ""
        return 0
    else
        echo -e "${RED}❌ Server is not running at $SERVER_URL${NC}"
        echo -e "${YELLOW}💡 Start the server with: make test-server${NC}"
        echo ""
        exit 1
    fi
}

# Main test execution
main() {
    check_server

    echo -e "${BLUE}🚀 Running Comprehensive ONVIF Tests${NC}"
    echo "====================================="
    echo ""

    # Device Service Tests
    echo -e "${BLUE}📱 Device Service Tests${NC}"
    echo "======================="

    run_soap_test "Device GetCapabilities" "device_service" "$SCRIPT_DIR/test_getcapabilities.xml" "RelayOutputs"
    run_soap_test "Device GetServices" "device_service" "$SCRIPT_DIR/test_getservices.xml" "Service"
    run_soap_test "Device GetDeviceInformation" "device_service" "$REQUESTS_DIR/device_getdeviceinformation.xml" "Manufacturer"
    run_soap_test "Device GetSystemDateAndTime" "device_service" "$REQUESTS_DIR/device_getsystemdateandtime.xml" "UTCDateTime"
    run_soap_test "Device GetNetworkInterfaces" "device_service" "$REQUESTS_DIR/device_getnetworkinterfaces.xml" "NetworkInterface"
    run_soap_test "Device GetScopes" "device_service" "$REQUESTS_DIR/device_getscopes.xml" "Scopes"
    run_soap_test "Device GetUsers" "device_service" "$REQUESTS_DIR/device_getusers.xml" "User"
    run_soap_test "Device GetWsdlUrl" "device_service" "$REQUESTS_DIR/device_getwsdlurl.xml" "WsdlUrl"

    echo ""

    # DeviceIO Service Tests
    echo -e "${BLUE}🔌 DeviceIO Service Tests${NC}"
    echo "========================="

    run_soap_test "DeviceIO GetServiceCapabilities" "deviceio_service" "$SCRIPT_DIR/test_deviceio_getcapabilities.xml" "RelayOutputs"
    run_soap_test "DeviceIO GetRelayOutputs" "deviceio_service" "$SCRIPT_DIR/test_deviceio_getrelayoutputs.xml" "RelayOutputs"
    run_soap_test "DeviceIO GetRelayOutputOptions" "deviceio_service" "$REQUESTS_DIR/deviceio_getrelayoutputoptions.xml" "RelayOutputOptions"
    run_soap_test "DeviceIO GetVideoSources" "deviceio_service" "$REQUESTS_DIR/deviceio_getvideosources.xml" "VideoSources"
    run_soap_test "DeviceIO GetAudioSources" "deviceio_service" "$REQUESTS_DIR/deviceio_getaudiosources.xml" "AudioSources" "true"
    run_soap_test "DeviceIO GetAudioOutputs" "deviceio_service" "$REQUESTS_DIR/deviceio_getaudiooutputs.xml" "AudioOutputs" "true"

    echo ""

    # Media Service Tests
    echo -e "${BLUE}📹 Media Service Tests${NC}"
    echo "======================"

    run_soap_test "Media GetServiceCapabilities" "media_service" "$REQUESTS_DIR/media_getservicecapabilities.xml" "Capabilities"
    run_soap_test "Media GetProfiles" "media_service" "$REQUESTS_DIR/media_getprofiles.xml" "Profiles"
    run_soap_test "Media GetStreamUri" "media_service" "$REQUESTS_DIR/media_getstreamuri.xml" "MediaUri"
    run_soap_test "Media GetSnapshotUri" "media_service" "$REQUESTS_DIR/media_getsnapshoturi.xml" "MediaUri"
    run_soap_test "Media GetVideoSources" "media_service" "$REQUESTS_DIR/media_getvideosources.xml" "VideoSources"
    run_soap_test "Media GetAudioSources" "media_service" "$REQUESTS_DIR/media_getaudiosources.xml" "AudioSources" "true"

    echo ""

    # PTZ Service Tests (optional - may not be configured)
    echo -e "${BLUE}🎮 PTZ Service Tests${NC}"
    echo "==================="

    run_soap_test "PTZ GetServiceCapabilities" "ptz_service" "$REQUESTS_DIR/ptz_getservicecapabilities.xml" "Capabilities" "true"
    run_soap_test "PTZ GetNodes" "ptz_service" "$REQUESTS_DIR/ptz_getnodes.xml" "PTZNode" "true"
    run_soap_test "PTZ GetConfigurations" "ptz_service" "$REQUESTS_DIR/ptz_getconfigurations.xml" "PTZConfiguration" "true"
    run_soap_test "PTZ GetPresets" "ptz_service" "$REQUESTS_DIR/ptz_getpresets.xml" "Preset" "true"
    run_soap_test "PTZ GetStatus" "ptz_service" "$REQUESTS_DIR/ptz_getstatus.xml" "PTZStatus" "true"

    echo ""

    # Events Service Tests
    echo -e "${BLUE}📡 Events Service Tests${NC}"
    echo "======================="

    run_soap_test "Events GetServiceCapabilities" "events_service" "$REQUESTS_DIR/events_getservicecapabilities.xml" "Capabilities"
    run_soap_test "Events GetEventProperties" "events_service" "$REQUESTS_DIR/events_geteventproperties.xml" "TopicSet"
    run_soap_test "Events CreatePullPointSubscription" "events_service" "$REQUESTS_DIR/events_createpullpointsubscription.xml" "SubscriptionReference"

    echo ""

    # Summary
    echo -e "${BLUE}📊 Test Results Summary${NC}"
    echo "======================="
    echo -e "Tests passed:  ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed:  ${RED}$TESTS_FAILED${NC}"
    echo -e "Tests skipped: ${YELLOW}$TESTS_SKIPPED${NC}"
    echo -e "Total tests:   $((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))"
    echo ""

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}🎉 All critical tests passed!${NC}"
        if [ $TESTS_SKIPPED -gt 0 ]; then
            echo -e "${YELLOW}ℹ️  Some optional tests were skipped${NC}"
        fi
        exit 0
    else
        echo -e "${RED}❌ Some tests failed!${NC}"
        exit 1
    fi
}

# Run main function
main "$@"

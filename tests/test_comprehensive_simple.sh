#!/bin/bash

# Simple Comprehensive ONVIF Test Suite
# Tests all major ONVIF operations in a contained environment

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQUESTS_DIR="$SCRIPT_DIR/requests"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
SERVER_DIR="$PROJECT_ROOT/server"
TESTS_PASSED=0
TESTS_FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 Simple Comprehensive ONVIF Test Suite${NC}"
echo "=========================================="
echo ""

# Function to run direct test
run_test() {
    local test_name="$1"
    local service="$2"
    local xml_file="$3"

    echo -e "${YELLOW}Testing: $test_name${NC}"

    if [ ! -f "$xml_file" ]; then
        echo -e "${RED}❌ FAIL: Test file not found: $xml_file${NC}"
        ((TESTS_FAILED++))
        return 1
    fi

    # Set up environment for CGI
    export REQUEST_METHOD=POST
    export CONTENT_TYPE="application/soap+xml"
    export CONTENT_LENGTH=$(wc -c < "$xml_file")
    export SCRIPT_NAME="/cgi-bin/$service"

    # Run the server directly from server directory
    local full_response
    full_response=$(cd "$SERVER_DIR" && ../onvif_simple_server -c test_onvif.json < "$xml_file" 2>&1)

    # Extract SOAP response (after the HTTP headers)
    local response
    response=$(echo "$full_response" | sed -n '/<?xml/,$p')

    if [[ -z "$response" ]] || [[ "$response" == *"SOAP-ENV:Fault"* ]] || [[ "$full_response" == *"No such file or directory"* ]]; then
        echo -e "${RED}❌ FAIL: No response, SOAP fault, or server error${NC}"
        if [[ "$full_response" == *"No such file or directory"* ]]; then
            echo -e "${RED}   Server binary not found. Run 'make' first.${NC}"
        fi
        ((TESTS_FAILED++))
        return 1
    fi

    echo -e "${GREEN}✅ PASS: Got valid response${NC}"
    ((TESTS_PASSED++))
    return 0
}

# Main test execution
main() {
    # Check if binary exists
    if [ ! -f "$PROJECT_ROOT/onvif_simple_server" ]; then
        echo -e "${RED}❌ ONVIF server binary not found. Run 'make' first.${NC}"
        exit 1
    fi

    # Check if contained environment is set up
    if [ ! -f "$SERVER_DIR/test_onvif.json" ]; then
        echo -e "${RED}❌ Test environment not set up. Run 'make test' first.${NC}"
        exit 1
    fi

    echo -e "${BLUE}🚀 Running Simple ONVIF Tests${NC}"
    echo "=============================="
    echo ""

    # Device Service Tests
    echo -e "${BLUE}📱 Device Service Tests${NC}"
    run_test "Device GetCapabilities" "device_service" "$SCRIPT_DIR/test_getcapabilities.xml"
    run_test "Device GetServices" "device_service" "$SCRIPT_DIR/test_getservices.xml"
    run_test "Device GetDeviceInformation" "device_service" "$REQUESTS_DIR/device_getdeviceinformation.xml"
    run_test "Device GetSystemDateAndTime" "device_service" "$REQUESTS_DIR/device_getsystemdateandtime.xml"

    echo ""

    # DeviceIO Service Tests
    echo -e "${BLUE}🔌 DeviceIO Service Tests${NC}"
    run_test "DeviceIO GetServiceCapabilities" "deviceio_service" "$SCRIPT_DIR/test_deviceio_getcapabilities.xml"
    run_test "DeviceIO GetRelayOutputs" "deviceio_service" "$SCRIPT_DIR/test_deviceio_getrelayoutputs.xml"

    echo ""

    # Media Service Tests
    echo -e "${BLUE}📹 Media Service Tests${NC}"
    run_test "Media GetServiceCapabilities" "media_service" "$REQUESTS_DIR/media_getservicecapabilities.xml"
    run_test "Media GetProfiles" "media_service" "$REQUESTS_DIR/media_getprofiles.xml"

    echo ""

    # Events Service Tests
    echo -e "${BLUE}📡 Events Service Tests${NC}"
    run_test "Events GetServiceCapabilities" "events_service" "$REQUESTS_DIR/events_getservicecapabilities.xml"

    echo ""

    # Summary
    echo -e "${BLUE}📊 Test Results Summary${NC}"
    echo "======================="
    echo -e "Tests passed:  ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed:  ${RED}$TESTS_FAILED${NC}"
    echo -e "Total tests:   $((TESTS_PASSED + TESTS_FAILED))"
    echo ""

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}🎉 All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}❌ Some tests failed!${NC}"
        exit 1
    fi
}

# Run main function
main "$@"

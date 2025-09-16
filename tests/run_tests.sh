#!/bin/bash

# ONVIF Simple Server Test Suite
# This script runs various ONVIF SOAP requests to test server functionality

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_URL="http://localhost:8000"
TESTS_PASSED=0
TESTS_FAILED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 ONVIF Simple Server Test Suite${NC}"
echo "=================================="
echo ""

# Function to run a SOAP test
run_soap_test() {
    local test_name="$1"
    local service="$2"
    local xml_file="$3"
    local expected_pattern="$4"

    echo -e "${YELLOW}Testing: $test_name${NC}"
    echo "Service: $service"
    echo "Request: $xml_file"

    if [ ! -f "$xml_file" ]; then
        echo -e "${RED}❌ FAIL: Test file not found: $xml_file${NC}"
        ((TESTS_FAILED++))
        echo ""
        return 1
    fi

    # Make the SOAP request
    local response
    response=$(curl -s -X POST \
        -H "Content-Type: application/soap+xml" \
        -H "SOAPAction: \"\"" \
        -d @"$xml_file" \
        "$SERVER_URL/onvif/$service" 2>/dev/null || echo "ERROR")

    if [[ "$response" == "ERROR" ]] || [[ -z "$response" ]]; then
        echo -e "${RED}❌ FAIL: No response or error from server${NC}"
        ((TESTS_FAILED++))
        echo ""
        return 1
    fi

    echo "$response"

    # Check if response contains expected pattern
    if [[ -n "$expected_pattern" ]] && [[ "$response" =~ $expected_pattern ]]; then
        echo -e "${GREEN}✅ PASS: Response contains expected pattern${NC}"
        ((TESTS_PASSED++))
    elif [[ -n "$expected_pattern" ]]; then
        echo -e "${RED}❌ FAIL: Response does not contain expected pattern: $expected_pattern${NC}"
        echo "Response preview: ${response:0:200}..."
        ((TESTS_FAILED++))
        echo ""
        return 1
    else
        echo -e "${GREEN}✅ PASS: Got valid response${NC}"
        ((TESTS_PASSED++))
    fi

    # Show key parts of response
    if [[ "$response" =~ \<tt:RelayOutputs\>([^<]*)\</tt:RelayOutputs\> ]]; then
        echo -e "📊 RelayOutputs: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:VideoSources\>([^<]*)\</tt:VideoSources\> ]]; then
        echo -e "📊 VideoSources: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    if [[ "$response" =~ \<tt:AudioSources\>([^<]*)\</tt:AudioSources\> ]]; then
        echo -e "📊 AudioSources: ${GREEN}${BASH_REMATCH[1]}${NC}"
    fi

    echo ""
    return 0
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
        echo -e "${YELLOW}💡 Start the server with: ./server/start_test_server.sh${NC}"
        echo ""
        exit 1
    fi
}

# Function to extract and show relay count from config
show_config_info() {
    echo -e "${BLUE}📋 Configuration Information${NC}"
    echo "=============================="

    local config_file="../config/relays.json"
    if [ -f "$config_file" ]; then
        local relay_count
        relay_count=$(jq '. | length' "$config_file" 2>/dev/null || echo "unknown")
        echo -e "Relays in config: ${GREEN}$relay_count${NC}"

        if command -v jq >/dev/null 2>&1; then
            echo "Relay details:"
            jq -r '.[] | "  - \(.close) / \(.open) (idle: \(.idle_state))"' "$config_file" 2>/dev/null || echo "  (unable to parse)"
        fi
    else
        echo -e "Relays config: ${RED}not found${NC}"
    fi
    echo ""
}

# Main test execution
main() {
    check_server
    show_config_info

    echo -e "${BLUE}🚀 Running ONVIF Tests${NC}"
    echo "====================="
    echo ""

    # Test 1: Device GetCapabilities
    run_soap_test \
        "Device GetCapabilities" \
        "device_service" \
        "$SCRIPT_DIR/test_getcapabilities.xml" \
        "RelayOutputs"

    # Test 2: Device GetServices
    run_soap_test \
        "Device GetServices" \
        "device_service" \
        "$SCRIPT_DIR/test_getservices.xml" \
        "RelayOutputs"

    # Test 3: DeviceIO GetServiceCapabilities
    run_soap_test \
        "DeviceIO GetServiceCapabilities" \
        "deviceio_service" \
        "$SCRIPT_DIR/test_deviceio_getcapabilities.xml" \
        "RelayOutputs"

    # Test 4: DeviceIO GetRelayOutputs
    run_soap_test \
        "DeviceIO GetRelayOutputs" \
        "deviceio_service" \
        "$SCRIPT_DIR/test_deviceio_getrelayoutputs.xml" \
        "RelayOutputs"

    # Summary
    echo -e "${BLUE}📊 Test Results Summary${NC}"
    echo "======================="
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    echo -e "Total tests:  $((TESTS_PASSED + TESTS_FAILED))"
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

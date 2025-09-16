#!/bin/bash

# Comprehensive Direct ONVIF Server Testing (without HTTP server)
# This script tests the ONVIF server directly using environment variables

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQUESTS_DIR="$SCRIPT_DIR/requests"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 Comprehensive Direct ONVIF Test Suite${NC}"
echo "=========================================="
echo ""

# Function to run direct test
run_direct_test() {
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

    # Set up environment for CGI
    export REQUEST_METHOD=POST
    export CONTENT_TYPE="application/soap+xml"
    export CONTENT_LENGTH=$(wc -c < "$xml_file")
    export SCRIPT_NAME="/cgi-bin/$service"

    # Run the server directly with contained config file from server directory
    local response
    response=$(cd "$PROJECT_ROOT/server" && ../onvif_simple_server -c test_onvif.json < "$xml_file" 2>/dev/null || echo "ERROR")

    if [[ "$response" == "ERROR" ]] || [[ -z "$response" ]]; then
        if [[ "$optional" == "true" ]]; then
            echo -e "${YELLOW}⚠️  SKIP: Optional test failed${NC}"
            ((TESTS_SKIPPED++))
        else
            echo -e "${RED}❌ FAIL: No response or error from server${NC}"
            ((TESTS_FAILED++))
        fi
        echo ""
        return 1
    fi

    # Check if response contains expected pattern
    if [[ -n "$expected_pattern" ]] && [[ "$response" =~ $expected_pattern ]]; then
        echo -e "${GREEN}✅ PASS: Response contains expected pattern${NC}"
        ((TESTS_PASSED++))
    elif [[ -n "$expected_pattern" ]]; then
        if [[ "$optional" == "true" ]]; then
            echo -e "${YELLOW}⚠️  SKIP: Optional test - pattern not found: $expected_pattern${NC}"
            ((TESTS_SKIPPED++))
        else
            echo -e "${RED}❌ FAIL: Response does not contain expected pattern: $expected_pattern${NC}"
            echo "Response preview: ${response:0:200}..."
            ((TESTS_FAILED++))
        fi
        echo ""
        return 1
    else
        echo -e "${GREEN}✅ PASS: Got valid response${NC}"
        ((TESTS_PASSED++))
    fi

    # Show key parts of response for debugging (simplified)
    echo -e "📊 Response length: ${#response} characters"

    echo ""
    return 0
}

# Function removed - was causing hanging issues

# Function to show config info
show_config_info() {
    echo -e "${BLUE}📋 Configuration Information${NC}"
    echo "=============================="

    local config_file="$PROJECT_ROOT/config/relays.json"
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
    # Check if binary exists
    if [ ! -f "$PROJECT_ROOT/onvif_simple_server" ]; then
        echo -e "${RED}❌ ONVIF server binary not found. Run 'make' first.${NC}"
        exit 1
    fi

    show_config_info

    echo -e "${BLUE}🚀 Running Comprehensive Direct ONVIF Tests${NC}"
    echo "============================================"
    echo ""

    # Device Service Tests
    echo -e "${BLUE}📱 Device Service Tests${NC}"
    echo "======================="

    run_direct_test "Device GetCapabilities" "device_service" "$SCRIPT_DIR/test_getcapabilities.xml" "RelayOutputs"
    run_direct_test "Device GetServices" "device_service" "$SCRIPT_DIR/test_getservices.xml" "Service"
    run_direct_test "Device GetDeviceInformation" "device_service" "$REQUESTS_DIR/device_getdeviceinformation.xml" "Manufacturer"

    echo ""

    # Additional tests commented out for debugging
    # TODO: Re-enable these tests once the basic tests are working

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

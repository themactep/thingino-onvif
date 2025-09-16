#!/bin/bash

# Direct ONVIF Server Testing (without HTTP server)
# This script tests the ONVIF server directly using environment variables

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 ONVIF Direct Test Suite${NC}"
echo "============================"
echo ""

# Function to run direct test
run_direct_test() {
    local test_name="$1"
    local service="$2"
    local xml_file="$3"
    
    echo -e "${YELLOW}Testing: $test_name${NC}"
    echo "Service: $service"
    echo "Request: $xml_file"
    
    if [ ! -f "$xml_file" ]; then
        echo -e "${RED}❌ FAIL: Test file not found: $xml_file${NC}"
        echo ""
        return 1
    fi
    
    # Set up environment for CGI
    export REQUEST_METHOD=POST
    export CONTENT_TYPE="application/soap+xml"
    export CONTENT_LENGTH=$(wc -c < "$xml_file")
    
    # Run the server directly
    local response
    response=$(cd "$PROJECT_ROOT" && ./onvif_simple_server < "$xml_file" 2>/dev/null || echo "ERROR")
    
    if [[ "$response" == "ERROR" ]] || [[ -z "$response" ]]; then
        echo -e "${RED}❌ FAIL: No response or error from server${NC}"
        echo ""
        return 1
    fi
    
    echo -e "${GREEN}✅ PASS: Got valid response${NC}"
    
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
    
    echo -e "${BLUE}🚀 Running Direct ONVIF Tests${NC}"
    echo "============================="
    echo ""
    
    # Test 1: Device GetCapabilities
    run_direct_test \
        "Device GetCapabilities" \
        "device_service" \
        "$SCRIPT_DIR/test_getcapabilities.xml"
    
    echo -e "${GREEN}🎉 Direct tests completed!${NC}"
}

# Run main function
main "$@"

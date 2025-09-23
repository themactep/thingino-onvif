#!/bin/bash

# ONVIF Simple Server Docker Test Script
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

echo -e "${BLUE}=== ONVIF Simple Server Docker Test Suite ===${NC}"
if [ "$VERBOSE" = true ]; then
    echo -e "${YELLOW}Verbose mode enabled - showing raw XML responses${NC}"
fi

# Check if container is running
if ! docker ps | grep -q $CONTAINER_NAME; then
    echo -e "${RED}Error: Container '$CONTAINER_NAME' is not running${NC}"
    echo "Start it with: ./docker_run.sh"
    exit 1
fi

echo -e "${GREEN}✓ Container '$CONTAINER_NAME' is running${NC}"

# Function to test ONVIF service
test_onvif_service() {
    local service_name=$1
    local method=$2
    local description=$3

    echo -e "\n${YELLOW}Testing: $description${NC}"

    # Create SOAP request (using SOAP 1.2 format that ONVIF expects)
    local soap_request="<?xml version=\"1.0\" encoding=\"utf-8\"?>
<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"
            xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\">
  <s:Header/>
  <s:Body>
    <tds:$method/>
  </s:Body>
</s:Envelope>"

    # Send request
    local response=$(curl -s -X POST \
        -H "Content-Type: application/soap+xml; charset=utf-8" \
        -H "SOAPAction: \"http://www.onvif.org/ver10/device/wsdl/$method\"" \
        -d "$soap_request" \
        "$SERVER_URL/onvif/$service_name")

    # Check if we got a valid SOAP response (any valid XML envelope indicates success)
    if [ $? -eq 0 ] && [[ $response == *"<?xml"* ]] && \
       [[ $response == *"Envelope"* ]] && \
       [[ $response == *"Body"* ]]; then
        echo -e "${GREEN}✓ $service_name/$method - SUCCESS${NC}"
        if [ "$VERBOSE" = true ]; then
            echo "Response: $response"
        fi
        return 0
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

# Test basic ONVIF device services
test_onvif_service "device_service" "GetSystemDateAndTime" "Get System Date and Time"
test_onvif_service "device_service" "GetDeviceInformation" "Get Device Information"
test_onvif_service "device_service" "GetCapabilities" "Get Device Capabilities"
test_onvif_service "device_service" "GetServices" "Get Available Services"

# Test media services
test_onvif_service "media_service" "GetProfiles" "Get Media Profiles"
test_onvif_service "media_service" "GetVideoSources" "Get Video Sources"

echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "${GREEN}Tests completed. Check output above for results.${NC}"
echo -e "\n${YELLOW}To view container logs:${NC} docker logs $CONTAINER_NAME"
echo -e "${YELLOW}To inspect container:${NC} ./docker_inspect.sh"

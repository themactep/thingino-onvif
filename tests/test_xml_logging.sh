#!/bin/bash

# Test script for XML request/response logging feature
# This script tests the raw XML logging functionality

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
TEST_LOG_DIR="/tmp/onvif_xml_logs_test"
CONFIG_FILE="/tmp/test_onvif_xml_logging.json"
ONVIF_SERVER="./onvif_simple_server"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}ONVIF XML Logging Test${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if server binary exists
if [ ! -f "$ONVIF_SERVER" ]; then
    echo -e "${RED}Error: ONVIF server binary not found: $ONVIF_SERVER${NC}"
    echo -e "${YELLOW}Please build the server first: ./build.sh${NC}"
    exit 1
fi

# Create test log directory
echo -e "${YELLOW}Creating test log directory: $TEST_LOG_DIR${NC}"
rm -rf "$TEST_LOG_DIR"
mkdir -p "$TEST_LOG_DIR"

# Create test configuration file
echo -e "${YELLOW}Creating test configuration file: $CONFIG_FILE${NC}"
cat > "$CONFIG_FILE" << 'EOF'
{
    "camera": {
        "firmware_ver": "1.0.0",
        "hardware_id": "TESTHW",
        "manufacturer": "TestManufacturer",
        "model": "TestModel",
        "serial_num": "TEST123456"
    },
    "server": {
        "ifs": "eth0",
        "log_directory": "/tmp/onvif_xml_logs_test",
        "log_level": "DEBUG",
        "log_on_error_only": false,
        "password": "testpass",
        "port": 80,
        "username": "testuser"
    },
    "profiles": {
        "stream0": {
            "name": "TestProfile",
            "width": 1920,
            "height": 1080,
            "url": "rtsp://test/stream",
            "snapurl": "http://test/snap.jpg",
            "type": "H264"
        }
    }
}
EOF

# Test 1: Simple GetDeviceInformation request
echo ""
echo -e "${BLUE}Test 1: GetDeviceInformation (no authentication)${NC}"
echo -e "${BLUE}========================================${NC}"

SOAP_REQUEST='<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
    <SOAP-ENV:Header/>
    <SOAP-ENV:Body>
        <tds:GetDeviceInformation/>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>'

export CONTENT_LENGTH=${#SOAP_REQUEST}
export REQUEST_METHOD=POST
export CONTENT_TYPE="application/soap+xml; charset=utf-8"
export SERVER_NAME="192.168.1.100"
export SERVER_PORT="80"
export REMOTE_ADDR="192.168.1.200"

echo -e "${YELLOW}Sending request...${NC}"
echo "$SOAP_REQUEST" | $ONVIF_SERVER -c "$CONFIG_FILE" -d 5 > /dev/null 2>&1 || true

# Check if logs were created
echo -e "${YELLOW}Checking for log files...${NC}"
sleep 1

if [ -d "$TEST_LOG_DIR/192.168.1.200" ]; then
    echo -e "${GREEN}✓ IP directory created: $TEST_LOG_DIR/192.168.1.200${NC}"
    
    REQUEST_FILES=$(find "$TEST_LOG_DIR/192.168.1.200" -name "*_request.xml" | wc -l)
    RESPONSE_FILES=$(find "$TEST_LOG_DIR/192.168.1.200" -name "*_response.xml" | wc -l)
    
    echo -e "${GREEN}✓ Found $REQUEST_FILES request file(s)${NC}"
    echo -e "${GREEN}✓ Found $RESPONSE_FILES response file(s)${NC}"
    
    if [ $REQUEST_FILES -gt 0 ] && [ $RESPONSE_FILES -gt 0 ]; then
        echo -e "${GREEN}✓ Test 1 PASSED${NC}"
        
        # Show file contents
        echo ""
        echo -e "${YELLOW}Request file content:${NC}"
        cat "$TEST_LOG_DIR/192.168.1.200"/*_request.xml | head -20
        echo ""
        echo -e "${YELLOW}Response file content:${NC}"
        cat "$TEST_LOG_DIR/192.168.1.200"/*_response.xml | head -20
    else
        echo -e "${RED}✗ Test 1 FAILED: Log files not created${NC}"
    fi
else
    echo -e "${RED}✗ Test 1 FAILED: IP directory not created${NC}"
fi

# Test 2: Multiple requests from different IPs
echo ""
echo -e "${BLUE}Test 2: Multiple requests from different IPs${NC}"
echo -e "${BLUE}========================================${NC}"

for IP in "192.168.1.201" "192.168.1.202" "10.0.0.100"; do
    echo -e "${YELLOW}Sending request from $IP...${NC}"
    export REMOTE_ADDR="$IP"
    echo "$SOAP_REQUEST" | $ONVIF_SERVER -c "$CONFIG_FILE" -d 5 > /dev/null 2>&1 || true
    sleep 0.5
done

echo -e "${YELLOW}Checking for log directories...${NC}"
sleep 1

TOTAL_DIRS=$(find "$TEST_LOG_DIR" -mindepth 1 -maxdepth 1 -type d | wc -l)
echo -e "${GREEN}✓ Found $TOTAL_DIRS IP directories${NC}"

if [ $TOTAL_DIRS -ge 4 ]; then
    echo -e "${GREEN}✓ Test 2 PASSED${NC}"
else
    echo -e "${RED}✗ Test 2 FAILED: Expected at least 4 directories, found $TOTAL_DIRS${NC}"
fi

# Test 3: Verify timestamp matching
echo ""
echo -e "${BLUE}Test 3: Verify request/response timestamp matching${NC}"
echo -e "${BLUE}========================================${NC}"

export REMOTE_ADDR="192.168.1.203"
echo "$SOAP_REQUEST" | $ONVIF_SERVER -c "$CONFIG_FILE" -d 5 > /dev/null 2>&1 || true
sleep 1

if [ -d "$TEST_LOG_DIR/192.168.1.203" ]; then
    REQUEST_FILE=$(find "$TEST_LOG_DIR/192.168.1.203" -name "*_request.xml" | head -1)
    RESPONSE_FILE=$(find "$TEST_LOG_DIR/192.168.1.203" -name "*_response.xml" | head -1)
    
    if [ -n "$REQUEST_FILE" ] && [ -n "$RESPONSE_FILE" ]; then
        REQUEST_TIMESTAMP=$(basename "$REQUEST_FILE" | sed 's/_request.xml//')
        RESPONSE_TIMESTAMP=$(basename "$RESPONSE_FILE" | sed 's/_response.xml//')
        
        echo -e "${YELLOW}Request timestamp:  $REQUEST_TIMESTAMP${NC}"
        echo -e "${YELLOW}Response timestamp: $RESPONSE_TIMESTAMP${NC}"
        
        if [ "$REQUEST_TIMESTAMP" = "$RESPONSE_TIMESTAMP" ]; then
            echo -e "${GREEN}✓ Test 3 PASSED: Timestamps match${NC}"
        else
            echo -e "${RED}✗ Test 3 FAILED: Timestamps don't match${NC}"
        fi
    else
        echo -e "${RED}✗ Test 3 FAILED: Log files not found${NC}"
    fi
else
    echo -e "${RED}✗ Test 3 FAILED: IP directory not created${NC}"
fi

# Test 4: Verify XML logging disabled when log_level is not DEBUG
echo ""
echo -e "${BLUE}Test 4: Verify logging disabled when log_level < DEBUG${NC}"
echo -e "${BLUE}========================================${NC}"

# Create config with INFO log level
cat > "$CONFIG_FILE" << 'EOF'
{
    "camera": {
        "firmware_ver": "1.0.0",
        "hardware_id": "TESTHW",
        "manufacturer": "TestManufacturer",
        "model": "TestModel",
        "serial_num": "TEST123456"
    },
    "server": {
        "ifs": "eth0",
        "log_directory": "/tmp/onvif_xml_logs_test",
        "log_level": "INFO",
        "password": "testpass",
        "port": 80,
        "username": "testuser"
    },
    "profiles": {
        "stream0": {
            "name": "TestProfile",
            "width": 1920,
            "height": 1080,
            "url": "rtsp://test/stream",
            "type": "H264"
        }
    }
}
EOF

export REMOTE_ADDR="192.168.1.204"
BEFORE_COUNT=$(find "$TEST_LOG_DIR" -name "*.xml" | wc -l)
echo "$SOAP_REQUEST" | $ONVIF_SERVER -c "$CONFIG_FILE" -d 3 > /dev/null 2>&1 || true
sleep 1
AFTER_COUNT=$(find "$TEST_LOG_DIR" -name "*.xml" | wc -l)

if [ $BEFORE_COUNT -eq $AFTER_COUNT ]; then
    echo -e "${GREEN}✓ Test 4 PASSED: No new logs created with INFO level${NC}"
else
    echo -e "${RED}✗ Test 4 FAILED: Logs were created despite INFO level${NC}"
fi

# Test 5: Verify logging disabled when log_directory is empty
echo ""
echo -e "${BLUE}Test 5: Verify logging disabled when log_directory is empty${NC}"
echo -e "${BLUE}========================================${NC}"

cat > "$CONFIG_FILE" << 'EOF'
{
    "camera": {
        "firmware_ver": "1.0.0",
        "hardware_id": "TESTHW",
        "manufacturer": "TestManufacturer",
        "model": "TestModel",
        "serial_num": "TEST123456"
    },
    "server": {
        "ifs": "eth0",
        "log_directory": "",
        "log_level": "DEBUG",
        "password": "testpass",
        "port": 80,
        "username": "testuser"
    },
    "profiles": {
        "stream0": {
            "name": "TestProfile",
            "width": 1920,
            "height": 1080,
            "url": "rtsp://test/stream",
            "type": "H264"
        }
    }
}
EOF

export REMOTE_ADDR="192.168.1.205"
BEFORE_COUNT=$(find "$TEST_LOG_DIR" -name "*.xml" | wc -l)
echo "$SOAP_REQUEST" | $ONVIF_SERVER -c "$CONFIG_FILE" -d 5 > /dev/null 2>&1 || true
sleep 1
AFTER_COUNT=$(find "$TEST_LOG_DIR" -name "*.xml" | wc -l)

if [ $BEFORE_COUNT -eq $AFTER_COUNT ]; then
    echo -e "${GREEN}✓ Test 5 PASSED: No new logs created with empty log_directory${NC}"
else
    echo -e "${RED}✗ Test 5 FAILED: Logs were created despite empty log_directory${NC}"
fi

# Summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Test Summary${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""
echo -e "${YELLOW}Total log files created: $(find "$TEST_LOG_DIR" -name "*.xml" | wc -l)${NC}"
echo -e "${YELLOW}Total IP directories: $(find "$TEST_LOG_DIR" -mindepth 1 -maxdepth 1 -type d | wc -l)${NC}"
echo ""
echo -e "${YELLOW}Log directory structure:${NC}"
tree "$TEST_LOG_DIR" 2>/dev/null || find "$TEST_LOG_DIR" -type f -o -type d | sort

# Cleanup
echo ""
echo -e "${YELLOW}Cleaning up test files...${NC}"
rm -rf "$TEST_LOG_DIR"
rm -f "$CONFIG_FILE"

echo ""
echo -e "${GREEN}All tests completed!${NC}"


#!/bin/bash

# ONVIF Simple Server Test Environment
# This script sets up a local CGI test server for testing ONVIF functionality

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SERVER_DIR="$SCRIPT_DIR/www"
PORT=8080

echo "🚀 Setting up ONVIF Test Server..."
echo "Project root: $PROJECT_ROOT"
echo "Server directory: $SERVER_DIR"
echo ""
echo "🌐 Starting HTTP server on port $PORT..."
echo "   Server URL: http://localhost:$PORT"
echo "   ONVIF services available at: http://localhost:$PORT/onvif/"
echo ""
echo "📋 Available ONVIF services:"
echo "   • Device Service:   http://localhost:$PORT/onvif/device_service"
echo "   • DeviceIO Service: http://localhost:$PORT/onvif/deviceio_service"
echo "   • Events Service:   http://localhost:$PORT/onvif/events_service"
echo "   • Media Service:    http://localhost:$PORT/onvif/media_service"
echo "   • Media2 Service:   http://localhost:$PORT/onvif/media2_service"
echo "   • PTZ Service:      http://localhost:$PORT/onvif/ptz_service"
echo ""
echo "🧪 Use the test scripts in server/tests/ to test functionality"
echo "   Press Ctrl+C to stop the server"
echo ""

# Change to server directory and start Python CGI server
cd "$SERVER_DIR"
lighttpd -D -f lighttpd.conf

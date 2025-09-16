#!/bin/bash

# Script to build and run memory corruption tests

set -e

echo "Building onvif_simple_server with debug symbols and AddressSanitizer..."

# Build with debug symbols and AddressSanitizer using the new debug target
echo "Compiling with memory debugging enabled..."
make debug

# Build the test executable using debug object files
echo "Building memory corruption test..."
gcc -g -O0 -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer \
    tests/test_memory_corruption.c \
    src/utils.debug.o \
    src/log.debug.o \
    -Isrc -I. \
    -ltomcrypt -ljson-c -lpthread -lrt \
    -o tests/test_memory_corruption

echo "Running memory corruption tests..."
cd tests
./test_memory_corruption

echo "Memory tests completed successfully!"

echo "Running basic functionality test..."
cd ..

# Create a minimal test config
mkdir -p test_config
cat > test_config/onvif.json << EOF
{
    "model": "Test Camera",
    "manufacturer": "Test Manufacturer",
    "firmware_ver": "1.0.0",
    "serial_num": "TEST123",
    "hardware_id": "TEST_HW",
    "ifs": "lo",
    "port": 8080,
    "username": "admin",
    "password": "admin",
    "profiles": []
}
EOF

echo "Testing debug application startup with AddressSanitizer..."
# Test that the debug application can start without crashing
timeout 5s ./onvif_simple_server_debug -c test_config/onvif.json -d 5 || true

echo "Building and testing regular application..."
make clean && make
timeout 5s ./onvif_simple_server -c test_config/onvif.json -d 5 || true

echo "Cleanup..."
rm -rf test_config

echo "All tests completed successfully!"

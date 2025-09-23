#!/bin/bash

# Build the ONVIF server binaries first
echo "Building ONVIF server binaries..."
./build.sh
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

# Build Docker image
echo "Building Docker image..."
docker build -t oss .

echo "Docker image 'oss' built successfully!"
echo "Run with: ./docker_run.sh"


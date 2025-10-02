#!/bin/bash

# Build the ONVIF server binaries first
echo "Building ONVIF server binaries..."
./build.sh
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Building podman image..."
cd container

echo "Copying resources..."
rsync -va ../res/* ./
cp ../onvif_notify_server ./
cp ../onvif_simple_server ./
cp ../wsd_simple_server ./

echo "Updating interface configuration for container environment..."
sed -i 's/"ifs": "wlan0"/"ifs": "lo"/g' onvif.json

podman build -t oss .

echo "podman image 'oss' built successfully!"
echo "Run with: ./container_run.sh"


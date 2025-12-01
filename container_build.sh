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

echo "Updating config for container environment..."

# Set network instaface
#sed -i 's/"ifs": "enp174s0"/"ifs": "lo"/g' onvif.json

# Enable PTZ
sed -i 's/"enable": 0,/"enable": 1,/' onvif.json

# Fake motors response
sed -i 's|"jump_to_abs": null|"jump_to_abs": "/bin/echo PTZ move to x=%f y=%f z=%f"|' onvif.json

podman build -t oss .

echo "podman image 'oss' built successfully!"
echo "Run with: ./container_run.sh"
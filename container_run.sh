#!/bin/bash

# Stop and remove any previous container with the same name
podman stop oss
podman rm oss

# Ensure host-side raw_logs directory exists and is writable
mkdir -p "$(pwd)/artifacts/lighttpd_logs"
mkdir -p "$(pwd)/artifacts/raw_logs"
#chmod 0775 "$(pwd)/artifacts/raw_logs" || true

# Run the new container with host networking
# This allows the container to see the host's network interfaces and IP addresses
# Mount the raw_logs directory so server logs are accessible on the host
podman run -d --replace \
    --name oss \
    --network host \
    --privileged \
    -v "$(pwd)/artifacts/lighttpd_logs:/var/www/lighttpd" \
    -v "$(pwd)/artifacts/raw_logs:/var/www/onvif/raw_logs" \
    oss
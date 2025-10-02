#!/bin/sh

# Stop and remove any previous container with the same name
podman stop oss
podman rm oss

# Run the new container with host networking
# This allows the container to see the host's network interfaces and IP addresses
podman run --name oss --network host -d oss


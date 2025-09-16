#!/bin/sh

# Stop and remove any previous container with the same name
docker stop onvif-lighttpd-server
docker rm onvif-lighttpd-server

# Run the new container, naming it explicitly
docker run --name onvif-lighttpd-server -p 8000:8000 onvif-lighttpd-server


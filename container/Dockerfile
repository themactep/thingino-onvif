# Use a lightweight Debian-based image
FROM debian:stable-slim

# Install Lighttpd and the CGI module
RUN apt-get update && apt-get install -y lighttpd libcjson1 libtomcrypt1 mc libcom-err2

# Create the directory for your CGI binaries and symlinks inside the container
WORKDIR /app/
RUN mkdir cgi-bin

# Copy binaries into the container
COPY onvif_notify_server      /app/cgi-bin/
COPY onvif_simple_server      /app/cgi-bin/
COPY wsd_simple_server        /app/cgi-bin/

# Copy template files for main ONVIF server (to CGI directory)
COPY device_service_files     /app/cgi-bin/device_service_files/
COPY deviceio_service_files   /app/cgi-bin/deviceio_service_files/
COPY events_service_files     /app/cgi-bin/events_service_files/
COPY generic_files            /app/cgi-bin/generic_files/
COPY media_service_files      /app/cgi-bin/media_service_files/
COPY media2_service_files     /app/cgi-bin/media2_service_files/
COPY ptz_service_files        /app/cgi-bin/ptz_service_files/

# Copy template files for notify server (to its expected directory)
COPY notify_files             /etc/onvif_notify_server/

# Copy template files for WSD server (to its expected directory)
COPY wsd_files                /etc/wsd_simple_server/

# Copy config file
COPY onvif.json               /etc/onvif.json

# Change the ownership of the files to the www-data user
# This is the user that Lighttpd runs as on Debian
RUN chown -R www-data:www-data /app/cgi-bin

# Grant execute permissions to your C apps
RUN chmod +x \
    /app/cgi-bin/onvif_simple_server \
    /app/cgi-bin/onvif_notify_server \
    /app/cgi-bin/wsd_simple_server

# Create CGI wrapper scripts
RUN echo '#!/bin/bash\nexec /app/cgi-bin/onvif_simple_server -c /etc/onvif.json device_service' > /app/cgi-bin/device_service.cgi && \
    echo '#!/bin/bash\nexec /app/cgi-bin/onvif_simple_server -c /etc/onvif.json media_service' > /app/cgi-bin/media_service.cgi && \
    echo '#!/bin/bash\nexec /app/cgi-bin/onvif_simple_server -c /etc/onvif.json media2_service' > /app/cgi-bin/media2_service.cgi && \
    echo '#!/bin/bash\nexec /app/cgi-bin/onvif_simple_server -c /etc/onvif.json ptz_service' > /app/cgi-bin/ptz_service.cgi && \
    echo '#!/bin/bash\nexec /app/cgi-bin/onvif_simple_server -c /etc/onvif.json deviceio_service' > /app/cgi-bin/deviceio_service.cgi && \
    echo '#!/bin/bash\nexec /app/cgi-bin/onvif_simple_server -c /etc/onvif.json events_service' > /app/cgi-bin/events_service.cgi && \
    chmod +x /app/cgi-bin/*.cgi

# Copy your custom lighttpd configuration file
COPY lighttpd.conf /etc/lighttpd/lighttpd.conf

# Expose the port for the web server
EXPOSE 8000

# Command to run the Lighttpd server when the container starts
CMD ["lighttpd", "-D", "-f", "/etc/lighttpd/lighttpd.conf"]

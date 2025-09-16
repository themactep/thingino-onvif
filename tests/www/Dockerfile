# Use a lightweight Debian-based image
FROM debian:stable-slim

# Install Lighttpd and the CGI module
RUN apt-get update && apt-get install -y lighttpd libjson-c5 libtomcrypt1

# Create the directory for your CGI binaries and symlinks inside the container
WORKDIR /app/
RUN mkdir cgi-bin

# Copy your binaries and the symlinks into the container
COPY cgi-bin/ /app/cgi-bin/
COPY xml/     /app/cgi-bin/

# Change the ownership of the files to the www-data user
# This is the user that Lighttpd runs as on Debian
RUN chown -R www-data:www-data /app/cgi-bin

# Grant execute permissions to your C apps
RUN chmod +x \
    /app/cgi-bin/onvif_simple_server \
    /app/cgi-bin/onvif_notify_server \
    /app/cgi-bin/wsd_simple_server

RUN ln -s /app/cgi-bin/onvif_simple_server /app/cgi-bin/device_service.cgi; \
    ln -s /app/cgi-bin/onvif_simple_server /app/cgi-bin/media_service.cgi; \
    ln -s /app/cgi-bin/onvif_simple_server /app/cgi-bin/media2_service.cgi; \
    ln -s /app/cgi-bin/onvif_simple_server /app/cgi-bin/ptz_service.cgi; \
    ln -s /app/cgi-bin/onvif_simple_server /app/cgi-bin/deviceio_service.cgi; \
    ln -s /app/cgi-bin/onvif_simple_server /app/cgi-bin/events_service.cgi

# Copy your custom lighttpd configuration file
COPY lighttpd.conf /etc/lighttpd/lighttpd.conf

# Expose the port for the web server
EXPOSE 8000

# Command to run the Lighttpd server when the container starts
CMD ["lighttpd", "-D", "-f", "/etc/lighttpd/lighttpd.conf"]

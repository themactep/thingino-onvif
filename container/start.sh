#!/bin/bash
# Container startup script: launch notify and discovery daemons, then lighttpd

# Notify server manages shared memory used by CreatePullPointSubscription.
/app/cgi-bin/onvif_notify_server -c /etc/onvif.json -f &

# WS-Discovery runs as the CGI user so SetDiscoveryMode can signal it.
su -s /bin/sh www-data -c \
    "nohup /app/cgi-bin/wsd_simple_server -i lo \
        -x 'http://%s:8000/onvif/device_service' \
        -c /etc/onvif.json \
        -p /tmp/wsd_simple_server_lo.pid \
        -f -d 5 >>/tmp/wsd_simple_server.log 2>&1 &"

# Give the daemons a moment to initialise shared memory and sockets.
sleep 1

exec lighttpd -D -f /etc/lighttpd/lighttpd.conf

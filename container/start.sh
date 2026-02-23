#!/bin/bash
# Container startup script: launch onvif_notify_server then lighttpd

# Start the notify server as a background daemon
# It manages shared memory used by CreatePullPointSubscription / PullMessages
/app/cgi-bin/onvif_notify_server -c /etc/onvif.json -f &

# Give it a moment to initialise shared memory
sleep 1

# Start lighttpd in the foreground (PID 1 substitute)
exec lighttpd -D -f /etc/lighttpd/lighttpd.conf

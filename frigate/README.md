A script to test ONVIF PTZ camera compatibility with Frigate
[autotracking](https://docs.frigate.video/configuration/autotracking/).

Usage: fovtest.py [-h] [--ip IP] [--port PORT] [--user USER] [--password PASSWORD]
options:
  -h, --help           show this help message and exit
  --ip IP              Camera IP address (default: 192.168.88.111)
  --port PORT          ONVIF port (default: 80)
  --user USER          Username (default: thingino)
  --password PASSWORD  Password (default: thingino)

./wsdl/ directory contains ONVIF files cleaned of trailing whitespaces,
with fixed paths for the script.

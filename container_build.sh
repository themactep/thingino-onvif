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

# Set network interface to loopback (always available in container)
sed -i 's/"ifs": "wlan0"/"ifs": "lo"/g' onvif.json

# Enable PTZ
sed -i 's/"enable": 0,/"enable": 1,/' onvif.json

# Enable Media2 for full test coverage
sed -i 's/"adv_enable_media2": false/"adv_enable_media2": true/' onvif.json

# Fake motors response
sed -i 's|"jump_to_abs": null|"jump_to_abs": "/bin/echo PTZ move to x=%f y=%f z=%f"|' onvif.json

# Use the test credentials and a stable serial so WSD and GetEndpointReference
# derive the same endpoint UUID.
python3 - <<'PY'
import json
path = "onvif.json"
with open(path) as f:
    config = json.load(f)
server = config.setdefault("server", {})
server["ifs"] = "lo"
server["username"] = "thingino"
server["password"] = "thingino"
config.setdefault("camera", {})["serial_num"] = "CONTAINER-TEST"
# TemperatureCritical is not in the stock config (it needs a thermal zone, T32
# and newer); add it here so the events suite covers it. No Source (8.9.10).
events = config.setdefault("events", [])
if not any(e.get("topic") == "tns1:Device/HardwareFailure/TemperatureCritical" for e in events):
    events.append({
        "input_file": "/run/motion/temperature_critical",
        "topic": "tns1:Device/HardwareFailure/TemperatureCritical",
    })
with open(path, "w") as f:
    json.dump(config, f, indent=2)
PY

podman build -t oss .

echo "podman image 'oss' built successfully!"
echo "Run with: ./container_run.sh"
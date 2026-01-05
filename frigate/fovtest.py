#!/bin/env python3
# This script can help you determine if your PTZ is capable of
# working with Frigate NVR's autotracker.
#
# Cameras with a "YES" printed for each parameter at the end of
# the output will likely be supported by Frigate.
#
# Make sure you're using python3 with the onvif-zeep package
# Update the values for your camera below, then run:
# pip3 install onvif-zeep
# python3 ./fovtest.py

# Modified for Thingino by Paul Philippov <paul@themactep.com>
# Unmodified script: https://gist.github.com/hawkeye217/152a1d4ba80760dac95d46e143d37112

import argparse
from onvif import ONVIFCamera

parser = argparse.ArgumentParser(description='Test ONVIF PTZ camera compatibility with Frigate')
parser.add_argument('--ip', default='192.168.88.111', help='Camera IP address (default: 192.168.88.111)')
parser.add_argument('--port', type=int, default=80, help='ONVIF port (default: 80)')
parser.add_argument('--user', default='thingino', help='Username (default: thingino)')
parser.add_argument('--password', default='thingino', help='Password (default: thingino)')
args = parser.parse_args()

mycam = ONVIFCamera(args.ip, args.port, args.user, args.password, 'wsdl/')

print('Connected to ONVIF camera')
# Create media service object
media = mycam.create_media_service()
print('Created media service object')
print
# Get target profile
media_profiles = media.GetProfiles()

print('Media profiles')
print(media_profiles)

for key, onvif_profile in enumerate(media_profiles):
    if (
        not onvif_profile.VideoEncoderConfiguration
        or onvif_profile.VideoEncoderConfiguration.Encoding != "H264"
    ):
        continue

    # Configure PTZ options
    if onvif_profile.PTZConfiguration:
        if onvif_profile.PTZConfiguration.DefaultContinuousPanTiltVelocitySpace is not None:
            media_profile = onvif_profile 

token = media_profile.token
print('Chosen token')
print(token)

print
# Create ptz service object
print('Creating PTZ object')
ptz = mycam.create_ptz_service()
print('Created PTZ service object')
print

# Get PTZ configuration options for getting option ranges
request = ptz.create_type("GetConfigurations")
configs = ptz.GetConfigurations(request)[0]
print('PTZ configurations:')
print(configs)
print()
request = ptz.create_type('GetConfigurationOptions')
request.ConfigurationToken = media_profile.PTZConfiguration.token
ptz_configuration_options = ptz.GetConfigurationOptions(request)
print('PTZ configuration options:')
print(ptz_configuration_options)
print()
print('PTZ service capabilities:')
request = ptz.create_type('GetServiceCapabilities')
service_capabilities = ptz.GetServiceCapabilities(request)
print(service_capabilities)
print()
print('PTZ status:')
request = ptz.create_type("GetStatus")
request.ProfileToken = token
status = ptz.GetStatus(request)
print(status)

pantilt_space_id = next(
    (
        i
        for i, space in enumerate(
            ptz_configuration_options.Spaces.RelativePanTiltTranslationSpace
        )
        if "TranslationSpaceFov" in space["URI"]
    ),
    None,
)

zoom_space_id = next(
    (
        i
        for i, space in enumerate(
            ptz_configuration_options.Spaces.RelativeZoomTranslationSpace
        )
        if "TranslationGenericSpace" in space["URI"]
    ),
    None,
)

def find_by_key(dictionary, target_key):
    if target_key in dictionary:
        return dictionary[target_key]
    else:
        for value in dictionary.values():
            if isinstance(value, dict):
                result = find_by_key(value, target_key)
                if result is not None:
                    return result
    return None        
        
if find_by_key(vars(service_capabilities), "MoveStatus"):
    print("YES - GetServiceCapabilities shows that the camera supports MoveStatus.")
else:
    print("NO - GetServiceCapabilities shows that the camera does not support MoveStatus.")

# there doesn't seem to be an onvif standard with this optional parameter
# some cameras can report MoveStatus with or without PanTilt or Zoom attributes
pan_tilt_status = getattr(status.MoveStatus, "PanTilt", None)
zoom_status = getattr(status.MoveStatus, "Zoom", None)

if pan_tilt_status is not None and pan_tilt_status == "IDLE" and (
    zoom_status is None or zoom_status == "IDLE"
):
    print("YES - MoveStatus is reporting IDLE.")

# if it's not an attribute, see if MoveStatus even exists in the status result
if pan_tilt_status is None:
    pan_tilt_status = getattr(status.MoveStatus, "MoveStatus", None)

    # we're unsupported
    if pan_tilt_status is None or (isinstance(pan_tilt_status, str) and pan_tilt_status not in [
        "IDLE",
        "MOVING",
    ]):
        print("NO - MoveStatus not reporting IDLE or MOVING.")

if pantilt_space_id is not None and configs.DefaultRelativePanTiltTranslationSpace is not None:
    print("YES - RelativeMove Pan/Tilt (FOV) is supported.")
else:
    print("NO - RelativeMove Pan/Tilt is unsupported.")
    
if zoom_space_id is not None:
    print("YES - RelativeMove Zoom is supported.")
else:
    print("NO - RelativeMove Zoom is unsupported.")

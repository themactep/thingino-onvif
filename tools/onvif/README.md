# ONVIF Camera Test Scripts

This folder contains small, standalone bash scripts to exercise a real ONVIF camera over HTTP. They help validate Media, Events (PullPoint), and DeviceIO (relay outputs) behavior, and are designed for quick smoke tests and repros.

Scripts do not run automatically. Execute them manually as needed.

## Contents

- lib_wsse.sh — Common helpers
  - Generates WS-Security UsernameToken PasswordDigest headers
  - Curl wrapper for SOAP POST requests
- test_media.sh — Media service checks
  - GetProfiles, GetStreamUri, GetSnapshotUri
- test_events.sh — Events service checks
  - GetServiceCapabilities, GetEventProperties
  - CreatePullPointSubscription + PullMessages loop
- test_deviceio.sh — DeviceIO checks
  - GetRelayOutputs (validates `tmd:` namespace)
  - Optional: pulse a relay token with `--pulse TOKEN`
- test_all.sh — Aggregated run of the above

## Requirements

- bash, curl, openssl
- coreutils (date, head, base64)
- Network access to the camera over HTTP

Note: Password digest is computed as Base64(SHA1(Nonce + Created + Password)) with a random 16-byte binary Nonce.

## Endpoints used

- Media:  http://HOST/onvif/media_service
- Events: http://HOST/onvif/events_service
- DeviceIO: http://HOST/onvif/deviceio_service

If your camera uses different paths (rare), adjust the scripts accordingly.

## Make scripts executable

```
chmod +x tools/onvif/*.sh
```

## Usage

Replace HOST/USER/PASS with your camera values.

### Media

Profiles, stream URIs, snapshot URIs:

```
tools/onvif/test_media.sh -h 192.168.88.116 -u thingino -p thingino
```

Expected output (excerpt):

```
Profiles (2):
Profile_0
Profile_1
StreamUri: rtsp://…/ch0
SnapshotUri: http://…/image.jpg
```

### DeviceIO (Relays)

List relays and (optionally) pulse a token:

```
# List
tools/onvif/test_deviceio.sh -h 192.168.88.116 -u thingino -p thingino

# Pulse a relay token
tools/onvif/test_deviceio.sh -h 192.168.88.116 -u thingino -p thingino --pulse RelayOutputToken_0
```

Expected output (excerpt):

```
==== GetRelayOutputs ====
tmd namespace OK: 1
Relay tokens:
RelayOutputToken_0
RelayOutputToken_1

==== SetRelayOutputState pulse token=RelayOutputToken_0 ====
SetRelayOutputState -> active
SetRelayOutputState -> inactive
```

Notes:
- For IR LED control in some apps (e.g., tinyCam), having DeviceIO relays correctly exposed with the `tmd:` namespace can be enough to surface an "IR LEDs" toggle.
- If your camera maps a relay to IR LEDs/IR-cut, identify which token is used by checking your configuration (e.g., onvif.json) or probing with `--pulse`.

### Events (PullPoint)

Capabilities, properties, and PullMessages loop:

```
tools/onvif/test_events.sh -h 192.168.88.116 -u thingino -p thingino -t 45
```

Expected output (excerpt):

```
==== GetServiceCapabilities ====
WSPullPointSupport: true

==== GetEventProperties ====
Motion topic present: 1
IsMotion schema present: 1

==== CreatePullPointSubscription ====
EPR: http://HOST/onvif/events_service?sub=42 (sub=42)

==== PullMessages for 45s (looking for Initialized/Changed IsMotion) ====
Initialized: 1, Changed(true): 1, Changed(false): 1
```

Tips:
- If motion events are not seen, trigger motion in front of the camera during the PullMessages loop.
- Some devices send an Initialized event with the current state immediately after subscription.

### Aggregate run

Run all tests in sequence:

```
tools/onvif/test_all.sh -h 192.168.88.116 -u thingino -p thingino --events-seconds 45
```

Optionally pulse a relay during DeviceIO test:

```
tools/onvif/test_all.sh -h 192.168.88.116 -u thingino -p thingino --events-seconds 45 --pulse RelayOutputToken_0
```

## Troubleshooting

- 401/Authentication errors
  - Ensure credentials are correct
  - Check device time skew; WS-Security uses the current UTC timestamp
- No profiles/URIs
  - Verify Media service is exposed and reachable
- Events: WSPullPointSupport missing/false
  - Ensure device advertises PullPoint support in Events capabilities
- Events: no motion topics or IsMotion schema
  - Device may not advertise motion in GetEventProperties; ensure configuration generates those topics, or update firmware to include a fallback topic declaration
- DeviceIO: wrong namespace (tds instead of tmd)
  - Clients may ignore responses unless DeviceIO responses use the `tmd:` namespace; update firmware accordingly

## tinyCam Monitor: IR LED controls

Apps like tinyCam may enable an "IR LEDs" control based on:
- DeviceIO relays (correct `tmd:` responses) that map to IR LED/IR-cut
- Imaging service IrCutFilter (tt:ImagingSettings20.IrCutFilter On/Off/Auto)

If IR controls do not appear:
- Confirm `test_deviceio.sh` shows relay tokens and `tmd` namespace OK: 1
- If still missing, app might depend on Imaging service; consider implementing minimal Imaging (Get/SetImagingSettings with IrCutFilter) and advertise it via Device:GetServices/GetCapabilities.

### Imaging service sanity check

Use `tools/onvif/test_imaging.sh -h <host> -u <user> -p <pass> [-t VideoSourceToken]` to exercise the Imaging service once it is enabled. The script performs:
- GetServiceCapabilities to confirm the endpoint is reachable.
- GetImagingSettings / GetOptions for the supplied video source token.
- SetImagingSettings with the currently reported IrCutFilter value (no-op) to verify write support.

## Notes

- These scripts are intended for development/QA against local devices. They do not modify system state other than optional relay pulses when requested.
- For debugging, you can echo full SOAP responses to files by replacing `-sS` with `-v` in `onvif_curl_post()` or redirecting outputs.


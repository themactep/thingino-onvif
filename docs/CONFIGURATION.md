# Configuration Guide

The ONVIF Simple Server reads a single JSON config file: `/etc/onvif.json`.
The only other file it touches is `/etc/onvif.d/preset_tours.json`, which holds
PTZ preset tours created at runtime.

## /etc/onvif.json (main)
```
{
  "server": {
    "ifs": "wlan0",
    "log_directory": "/var/www/onvif/raw",
    "log_level": "INFO",            // FATAL..TRACE or 0-5 for backward compatibility
    "log_on_error_only": false,
    "port": 80,
    "username": "",
    "password": ""
  },
  "scopes": [
    "onvif://www.onvif.org/Profile/Streaming",
    "onvif://www.onvif.org/Profile/T",
    "onvif://www.onvif.org/hardware",
    "onvif://www.onvif.org/name"
  ],
  "profiles": { ... },   // media profiles and stream URLs
  "ptz": { ... },        // PTZ control and command hooks
  "relays": [ ... ],     // relay outputs behavior and shell commands
  "imaging": [ ... ],    // imaging/IR-cut configuration
  "events": [ ... ]      // events support and file-driven inputs
}
```

Notes:
- Camera identity (manufacturer, model, firmware_ver, hardware_id, serial_num)
  is read from the system itself - /etc/os-release (NAME, IMAGE_ID, BUILD_ID),
  `soc -m` and `soc -s` - so it never goes stale. An optional `camera`
  section in this file overrides fields the system cannot provide.
- `server.ifs` is normally ignored: the primary interface is taken from the
  routing table (default route) at config load. This key is only a fallback
  when no default route exists.
- `server.username`/`server.password` are only a fallback: by default the
  credentials are read from the installed streamer's own config
  (/etc/prudynt.json, /etc/streamer.d/rtsp.json, /etc/timps.conf or
  /etc/raptor.conf), so ONVIF auth always matches RTSP auth. When the streamer
  has no RTSP user configured, ONVIF stays open.
- `server.port` selects the ONVIF listen port (usually 80, behind the web server).
- `server.log_directory` enables raw SOAP request/response XML logging; empty
  disables it.

Events are file-driven: the notify daemon watches `input_file` and emits a
notification when it appears/disappears. Each event carries one or more Source
items (the ONVIF catalog defines up to three, e.g. CellMotionDetector:
`VideoSourceConfigurationToken`, `VideoAnalyticsConfigurationToken`, `Rule`)
plus a Data item. The legacy single-item keys `source_name`/`source_type`/
`source_value` are still accepted.
```
"events": [
  {
    "input_file": "/run/motion/motion_alarm",
    "sources": [
      { "name": "Source", "type": "tt:ReferenceToken", "value": "VideoSourceToken" }
    ],
    "topic": "tns1:VideoSource/MotionAlarm"
  }
]
```

## /etc/onvif.d/preset_tours.json
Created and updated by the PTZ service at runtime (`ptz_service.c`). Holds the
preset tour definitions for `PresetTour` operations.

## Using jct (JSON Config Tool)
The Thingino init scripts use `jct` to create/update JSON entries.
```
jct /etc/onvif.json create
jct /etc/onvif.json set scopes '["onvif://www.onvif.org/Profile/Streaming"]'
```

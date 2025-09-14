# ONVIF Simple Server for Thingino

A lightweight ONVIF (Open Network Video Interface Forum) server designed for embedded cameras running the Thingino firmware. It exposes ONVIF Device, Media, Media2, PTZ, Events and DeviceIO services using simple XML templates and a modular JSON configuration.

## Highlights
- Modular configuration split between a main file and per‑feature modules
  - Main: `/etc/onvif.json`
  - Modules: `/etc/onvif.d/{profiles,ptz,relays,events}.json`
- Standards‑compliant SOAP over HTTP using ezXML parser and file templates
- Optional raw XML request/response logging to a file for deep debugging
- Syslog‑based logging with proper levels (FATAL..TRACE)
- Minimal dependencies suitable for constrained devices

## Binaries
- `onvif_simple_server` — multi‑service CGI binary (Device/Media/Media2/PTZ/Events/DeviceIO)
- `onvif_notify_server` — event injector (file‑driven)
- `wsd_simple_server` — WS‑Discovery responder (daemon)

## Directory Layout (Thingino image)
- Binaries: `/usr/sbin/{onvif_simple_server,onvif_notify_server,wsd_simple_server}`
- Web templates (served by lighttpd): `/var/www/onvif/*_files/*.xml`
  - In the source tree, templates live under `xml/*_service_files` and `xml/{generic,notify,wsd}_files`.
- Config (modular): `/etc/onvif.json`, `/etc/onvif.d/*.json`
- Init scripts: `/etc/init.d/S96onvif_discovery`, `/etc/init.d/S97onvif_notify`

## Quick Start
1) Ensure the example configs are installed (package does this by default):
- `/etc/onvif.json`
- `/etc/onvif.d/profiles.json`
- `/etc/onvif.d/ptz.json`
- `/etc/onvif.d/relays.json`
- `/etc/onvif.d/events.json`

2) Adjust values (model, RTSP URLs, PTZ and relays) to match your device.

3) Reboot or start services:
```
/etc/init.d/S96onvif_discovery start
/etc/init.d/S97onvif_notify start
```

4) Test discovery from a client (e.g., ONVIF Device Manager). The WS‑Discovery service announces the device; ONVIF operations are served by `onvif_simple_server` via HTTP.

## Raw XML Logging (optional)
Set `raw_xml_log_file` in `/etc/onvif.json` (default empty). Example:
```
{
  "raw_xml_log_file": "/tmp/onvif_raw.log"
}
```
This writes full SOAP requests and responses to that file, useful when syslog truncates long XML messages.

## Build and Integration
This repository integrates with the Thingino Buildroot package `onvif-simple-server`. All paths are defined directly in the sources/configs (no patching step). Templates remain under `*_service_files` directories.

See DEPLOYMENT.md for image integration and installed files, and CONFIGURATION.md for all config keys.


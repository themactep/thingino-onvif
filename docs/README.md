# ONVIF Simple Server for Thingino

A lightweight ONVIF (Open Network Video Interface Forum) server designed for embedded cameras running the Thingino firmware. It exposes ONVIF Device, Media, Media2, PTZ, Events and DeviceIO services using simple XML templates and a modular JSON configuration.

## Highlights
- Modular configuration split between a main file and per‑feature modules
  - Main: `/etc/onvif.json`
  - Modules: `/etc/onvif.d/{profiles,ptz,relays,events}.json`
- Standards‑compliant SOAP over HTTP using mxml parser and file templates
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


This writes full SOAP requests and responses to that file, useful when syslog truncates long XML messages.

## Build and Integration
This repository integrates with the Thingino Buildroot package `onvif-simple-server`. All paths are defined directly in the sources/configs (no patching step). Templates remain under `*_service_files` directories.

## Documentation Index

### 🏗️ Build & Setup
- **[BUILD.md](BUILD.md)** - Build instructions for development and production
- **[BUILDROOT_INTEGRATION.md](BUILDROOT_INTEGRATION.md)** - Buildroot package integration details
- **[BUILDROOT_PACKAGE_UPDATES.md](BUILDROOT_PACKAGE_UPDATES.md)** - Recent package configuration updates

### 🔄 Migration & Security
- **[MIGRATION_SUMMARY.md](MIGRATION_SUMMARY.md)** - Executive summary of ezxml→mxml migration
- **[MXML_MIGRATION.md](MXML_MIGRATION.md)** - Detailed technical migration guide

### ⚙️ Configuration & API
- **[CONFIGURATION.md](CONFIGURATION.md)** - Complete configuration schema and examples
- **[CONFIGURATION_SIMPLIFICATION.md](CONFIGURATION_SIMPLIFICATION.md)** - Configuration improvements
- **[API.md](API.md)** - ONVIF API endpoints and usage

### 🚀 Operations & Deployment
- **[DEPLOYMENT.md](DEPLOYMENT.md)** - Image integration and installed files
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - Common issues and solutions
- **[WSD_DEBUG_LOG_EXAMPLE.md](WSD_DEBUG_LOG_EXAMPLE.md)** - WS-Discovery debugging

### 🔧 Development & Maintenance
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and changes
- **[ENHANCED_LOGGING_README.md](ENHANCED_LOGGING_README.md)** - Logging improvements
- **[MEMORY_CORRUPTION_FIXES.md](MEMORY_CORRUPTION_FIXES.md)** - Memory safety fixes
- **[BETTER_PROBE_HANDLING.md](BETTER_PROBE_HANDLING.md)** - Probe handling improvements
- **[COMPRESSION.md](COMPRESSION.md)** - Filesystem-level compression approach

See the main [README.md](../README.md) for a quick overview and getting started guide.


# Deployment & Integration (Thingino)

This project is packaged as a Buildroot package for Thingino. Binaries, templates, and example configs are installed into the firmware image and started by init scripts.

## Installed Files
- Binaries (CGI or daemons):
  - `/usr/sbin/onvif_simple_server`
  - `/usr/sbin/onvif_notify_server`
  - `/usr/sbin/wsd_simple_server`
- Web templates (served by lighttpd):
  - `/var/www/onvif/device_service_files/*`
  - `/var/www/onvif/events_service_files/*`
  - `/var/www/onvif/generic_files/*`
  - `/var/www/onvif/media_service_files/*`
  - `/var/www/onvif/media2_service_files/*`
  - `/var/www/onvif/ptz_service_files/*`
  - `/var/www/onvif/wsd_files/*`
  - Note: in the source tree, these templates are under `xml/*` and are installed to `/var/www/onvif` by the Buildroot package.
- Configuration (modular):
  - `/etc/onvif.json` (main)
  - `/etc/onvif.d/profiles.json`
  - `/etc/onvif.d/ptz.json`
  - `/etc/onvif.d/relays.json`
  - `/etc/onvif.d/events.json`
- Init scripts:
  - `/etc/init.d/S96onvif_discovery`
  - `/etc/init.d/S97onvif_notify`

## Init Scripts
- `S96onvif_discovery` prepares configuration using `jct`, sets `conf_dir`, and adjusts PTZ defaults based on available hardware, then starts WS-Discovery.
- `S97onvif_notify` starts the event notification server which reads event inputs from files (e.g., `/tmp/onvif_notify_server/motion_alarm`).

## Configuration
See CONFIGURATION.md for the modular configuration structure. The package installs working examples to `/etc` and `/etc/onvif.d/`.

## Logging
- Syslog is used for operational logs.
- Optional full SOAP request/response logging: set `raw_xml_log_file` in `/etc/onvif.json`, e.g. `/tmp/onvif_raw.log`. Ensure writable storage (tmpfs, SD card, or network mount).
- `loglevel`: 0..5 (FATAL..TRACE). CLI `-d` may override the config for foreground debugging.

## Building within Thingino
Handled by the package `package/onvif-simple-server/onvif-simple-server.mk` which:
- Fetches upstream sources, overlays Thingino overrides, and builds via `make -C $(@D)`
- Installs binaries, templates, and example configs into the target image

No external patching is required; all paths are defined in the sources/configs.

## Service Endpoints
`onvif_simple_server` is invoked under different script names in `/var/www/onvif/` to provide Device, Media, Media2, PTZ, Events, and DeviceIO services. See API.md.


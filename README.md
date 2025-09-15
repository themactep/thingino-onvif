# Thingino ONVIF Simple Server (fork)

Fork of [roleoroleo’s ONVIF Simple Server](https://github.com/roleoroleo/onvif_simple_server), adapted for the Thingino firmware.
Small, dependency‑light, and integrated with Thingino’s build, config, and logging stack.

## What’s different vs upstream
- JSON‑only modular config (no INI)
  - /etc/onvif.json (main) + /etc/onvif.d/{profiles,ptz,relays,events}.json
  - Optional onvif.json: "conf_dir" (defaults to /etc/onvif.d)
  - Managed via jct where needed (init scripts)
- Syslog logging integration (0=fatal .. 5=trace). Loglevel from JSON or CLI
- Raw XML request/response mirroring to a file for SOAP troubleshooting
- Reorganized tree: src/, xml/, config/, docs/
- Thingino Buildroot package installs binaries, templates, JSON, init scripts

## Build (Thingino Buildroot)
- Package: package/onvif-simple-server
- Build: make onvif-simple-server
- Optional: zlib for compressed templates (disable/enable via BR config)

## Configure
- Main: /etc/onvif.json
- Modules: /etc/onvif.d/*.json (profiles, PTZ, relays, events)
- jct can update keys safely from init scripts
- See docs/CONFIGURATION.md for full schema and examples

## Debugging
- Logs: syslog (foreground prints to stderr)
- Raw SOAP: set onvif.json "raw_xml_log_file" (e.g., /tmp/onvif_raw.log)

## Templates
- XML templates installed under /var/www/onvif/* from the xml/ directory

## Documentation
- See docs/: README.md, CONFIGURATION.md, API.md, DEPLOYMENT.md, TROUBLESHOOTING.md, CHANGELOG.md

## Attribution & License
- Upstream author: roleoroleo — https://github.com/roleoroleo/onvif_simple_server
- This is a derivative, Thingino‑specific fork. License: GPLv3

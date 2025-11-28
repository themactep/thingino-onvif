# Changelog

## Unreleased
- Reorganized source tree: all C/H sources moved to `src/`
- XML templates relocated under `xml/` in the source tree; installed paths under `/var/www/onvif/*` unchanged
- Introduced `docs/` with comprehensive documentation:
  - README, CONFIGURATION, API, DEPLOYMENT, TROUBLESHOOTING, CHANGELOG
- Modular configuration system finalized:
  - Main: `/etc/onvif.json`
  - Modules: `/etc/onvif.d/{profiles,ptz,relays,events}.json`
- Syslog-based logging with proper levels (FATAL..TRACE); foreground respects CLI `-d`
- Removed legacy artifacts:
  - Monolithic `onvif_simple_server.json.example`
  - `extras/` directory contents not used by Thingino build (patch/build helpers)
- Cleaned up old references to `/etc/onvif_simple_server.json`; defaults now `/etc/onvif.json` and `/etc/onvif.d`
- Implemented ONVIF Imaging service (ver20) with IrCutFilter support, JSON configuration (`imaging` block), new CGI handlers, and `tools/onvif/test_imaging.sh` for validation.

## Migration Notes (Monolithic -> Modular)
- Config is no longer a single monolithic JSON file
- Split configuration as follows:
  - Device info, logging, global options: `/etc/onvif.json`
  - Media profiles (URLs, types, audio): `/etc/onvif.d/profiles.json`
  - PTZ: `/etc/onvif.d/ptz.json`
  - Relay outputs: `/etc/onvif.d/relays.json`
  - Events: `/etc/onvif.d/events.json`
- Init scripts (`S96onvif_discovery`, `S97onvif_notify`) updated to write modular files via `jct`
- No backward-compatibility layer is provided; images fully replace prior installations


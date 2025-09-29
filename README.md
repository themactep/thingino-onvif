# Thingino ONVIF Simple Server (fork)

Fork of [roleoroleo’s ONVIF Simple Server](https://github.com/roleoroleo/onvif_simple_server), adapted for the Thingino firmware.
Small, dependency‑light, and integrated with Thingino’s build, config, and logging stack.

## What’s different vs upstream
- **Security improvements**: Migrated from unmaintained ezxml to actively maintained mxml
- **JSON‑only modular config** (no INI)
  - /etc/onvif.json (main) + /etc/onvif.d/{profiles,ptz,relays,events}.json
  - Optional onvif.json: "conf_dir" (defaults to /etc/onvif.d)
  - Managed via jct where needed (init scripts)
- **Syslog logging integration** (0=fatal .. 5=trace). Loglevel from JSON or CLI
- **Raw XML request/response mirroring** to a file for SOAP troubleshooting
- **Reorganized tree**: src/, xml/, config/, docs/
- **Thingino Buildroot package** installs binaries, templates, JSON, init scripts
- **Proper Buildroot integration** with thingino-jct and mxml dependencies

## Build
- **Production (Thingino Buildroot)**:
    Package: package/thingino-onvif,
    Build: make rebuild-thingino-onvif
    Dependencies: thingino-jct, thingino-mxml v4.0.4 (automatically managed)
- **Local Development**:
    Use `./build.sh` script (builds jct and mxml libraries locally)
    Use `make distclean` to remove all local artifacts and repositories
    Run `./setup-hooks.sh` to install git hooks for automatic code formatting
- **Cross-Compilation**:
    `./build.sh mipsel-linux-` (toolchain must be in PATH)
- See [docs/BUILD.md](docs/BUILD.md) for detailed build instructions and cross-compilation setup
- Templates are served uncompressed, relying on squashfs filesystem compression

## Configure
- Main: /etc/onvif.json
- Modules: /etc/onvif.d/*.json (profiles, PTZ, relays, events)
- jct can update keys safely from init scripts
- See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for full schema and examples

## Debugging
- Logs: syslog (foreground prints to stderr)

## Templates
- XML templates installed under /var/www/onvif/* from the xml/ directory

## Documentation
- **Build & Setup**: [BUILD.md](docs/BUILD.md), [BUILDROOT_INTEGRATION.md](docs/BUILDROOT_INTEGRATION.md)
- **Migration**: [MIGRATION_SUMMARY.md](docs/MIGRATION_SUMMARY.md), [MXML_MIGRATION.md](docs/MXML_MIGRATION.md)
- **Configuration**: [CONFIGURATION.md](docs/CONFIGURATION.md), [API.md](docs/API.md)
- **Operations**: [DEPLOYMENT.md](docs/DEPLOYMENT.md), [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)
- **Development**: [CHANGELOG.md](docs/CHANGELOG.md), [README.md](docs/README.md), [GIT_HOOKS.md](docs/GIT_HOOKS.md)

## Attribution & License
- Upstream author: roleoroleo — https://github.com/roleoroleo/onvif_simple_server
- This is a derivative, Thingino‑specific fork. License: GPLv3

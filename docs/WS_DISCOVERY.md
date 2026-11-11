# WS-Discovery implementation notes

This fork follows the ONVIF Core specification 26.06, chapter 7, and the
WS-Discovery 2005/04 specification it normatively references.

## Target Service behaviour

`wsd_simple_server` implements the WS-Discovery Target Service role:

- **Hello**: multicast on link join and when discovery metadata changes.
- **Bye**: multicast before shutdown and when entering non-discoverable mode.
- **Probe / ProbeMatches**: unicast response only when all Probe types match
  and all Probe scopes match.
- **Resolve / ResolveMatches**: unicast response when the endpoint reference
  matches this device.
- **Duplicate suppression**: repeated Probe/Resolve copies with the same
  MessageID are answered once within a short window.
- **Application sequencing**: `AppSequence` uses a boot-time `InstanceId` and a
  monotonically increasing `MessageNumber`.
- **Endpoint reference**: a stable UUID derived from the device serial number or persistent
  device MAC, constant across interfaces and reboots. `GetEndpointReference` returns
  the same UUID to ONVIF clients.

Types advertise both `tdn:NetworkVideoTransmitter` and the ONVIF-required
`tds:Device`. Scopes come from the `scopes` array in `/etc/onvif.json`, so
WS-Discovery matches `GetScopes` and `GetCapabilities`.

## Message size

Hello, Bye and match messages must fit one UDP datagram. The daemon warns when
a rendered message exceeds 1232 bytes (the IPv6 minimum MTU payload). Keep the
`scopes` list short enough for one datagram.

## IPv4 and IPv6

The daemon auto-detects IPv4 and IPv6 addresses on the selected interface. When
an IPv6 address exists it uses a single dual-stack socket bound to `[::]:3702`,
joins `239.255.255.250` and `FF02::C` on that interface, and replies with the
address family of the sender. IPv6 is disabled gracefully when no usable
address or multicast join is available. `GetNetworkInterfaces` reports the
detected IPv6 addresses.

## Discovery mode

`GetDiscoveryMode` and `SetDiscoveryMode` control the discoverable /
non-discoverable state. The state is kept in `/tmp/onvif_discovery_mode` and the
running daemon is reloaded with `SIGHUP`:

- entering non-discoverable sends Bye and closes the discovery socket;
- returning to discoverable reopens the socket and sends Hello.

The default is `Discoverable`, as required by ONVIF Core. The state is not
persisted across reboot.

## Command line

```
wsd_simple_server -i INTERFACE -x XADDR [-6 XADDR6] [-m MODEL] [-n HARDWARE]
                  [-s SCOPES] [-c CONFIG] -p PID_FILE [-t TEMPLATE_DIR]
                  [-f] [-d LEVEL]
```

- `-x`: IPv4 service address template; `%s` becomes the detected IPv4 address.
- `-6`: optional IPv6 address template; defaults to `-x` with brackets.
- `-s`: overrides the scope list from the config file.
- `-c`: JSON config file, default `/etc/onvif.json`.
- `-t`: template directory, default `/var/www/onvif/wsd_files`.

If no `scopes` are configured, the daemon builds a minimal spec-compliant list
from the hardware, model, MAC address, serial number and
`onvif://www.onvif.org/Profile/Streaming`.

## Debugging

Run in the foreground with tracing:

```
wsd_simple_server -i eth0 -x http://%s/onvif/device_service -p /tmp/wsd.pid -f -d 5
```

The log shows parsed Probe/Resolve messages, type and scope match results, and
the rendered response size warning when a message exceeds 1232 bytes.

## Tests

- `tools/onvif/test_wsd_discovery.rb` starts the daemon and checks Hello,
  Probe type/scope matching, Resolve, AppSequence, EPR stability, duplicate
  suppression and discovery-mode transitions.
- `tools/onvif/test_discovery_mode.sh` checks `GetDiscoveryMode`,
  `SetDiscoveryMode` and IPv6 reporting through the ONVIF device service.
- `container_test.sh` runs the same checks through the test container,
  including live discoverable/non-discoverable transitions and endpoint
  reference consistency.

## Out of scope

Discovery Proxy behaviour, WS-Discovery message signing, and HTTPS XAddrs are
not implemented. Each address family advertises one service address.

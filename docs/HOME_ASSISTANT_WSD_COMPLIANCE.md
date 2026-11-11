# Home Assistant WS-Discovery compliance

Home Assistant probes for `tdn:NetworkVideoTransmitter` with the
`onvif://www.onvif.org/Profile/Streaming` scope and reads the device name,
hardware and MAC from the returned scopes.

The current implementation covers those requirements and the broader ONVIF
Core 26.06 discovery requirements:

- Hello, ProbeMatches and ResolveMatches carry
  `tdn:NetworkVideoTransmitter tds:Device`.
- The configured `scopes` list is advertised unchanged, so it matches
  `GetScopes`.
- The default fallback scope list includes `Profile/Streaming`, hardware,
  name, MAC, serial number and location.
- Probe types and scopes are matched before a response is sent.
- Resolve/ResolveMatches is implemented.
- IPv4 and IPv6 are supported.
- `AppSequence` uses a boot-time instance ID and monotonic message numbers.
- The endpoint reference is a stable device UUID derived from the serial number or persistent MAC.

See [WS_DISCOVERY.md](WS_DISCOVERY.md) for details.

The compliance test is `tools/onvif/test_wsd_discovery.rb`.

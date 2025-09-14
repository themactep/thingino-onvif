# Troubleshooting

## Discovery doesn’t find the device
- Ensure `wsd_simple_server` is running: `ps | grep wsd_simple_server`
- Check network interface in `/etc/onvif.json` (`ifs`): must match the active interface
- Verify multicast is allowed (239.255.255.250:3702/UDP)
- Inspect logs (syslog) for WS-Discovery errors

## Client fails ONVIF authentication
- If `user`/`password` are set in `/etc/onvif.json`, the server expects WS-Security UsernameToken with PasswordDigest
- Ensure client clock is reasonably synchronized (timestamps affect digest)
- Clear credentials in config to disable auth and retest

## RTSP or Snapshot URI invalid / authentication issues
- Check `profiles.json` URL templates use `%s` placeholder correctly (e.g. `rtsp://%s/ch0_0.h264`)
- If `user`/`password` are set in `/etc/onvif.json`, URIs are returned with embedded credentials (e.g. `rtsp://user:pass@host/...`)
- Confirm the RTSP server actually listens and accepts those credentials

## PTZ doesn’t move
- Ensure `/etc/onvif.d/ptz.json` has `enable: 1`
- Validate external PTZ scripts exist and are executable (e.g. `/usr/local/bin/ptz_move`)
- Use the ONVIF client to call PTZ operations and watch syslog for errors

## Events don’t trigger
- Confirm `/etc/onvif.d/events.json` has `enable: 1, 2, or 3`
- Write to the configured `input_file` and observe `onvif_notify_server` logs
- Check permissions on `/tmp/onvif_notify_server/*` files

## Template file not found
- The server assembles XML from files under `/var/www/onvif/*_files`
- Verify the path exists in the image and files are present

## Debugging with Raw XML Logging
Enable full request/response logging by setting in `/etc/onvif.json`:
```
{
  "raw_xml_log_file": "/tmp/onvif_raw.log",
  "loglevel": 5
}
```
Notes:
- Writes untruncated SOAP XML to the file (requests and responses)
- Use temporary or large storage (tmpfs, SD, or network share) to avoid flash wear
- Disable or rotate when finished debugging

## Increasing Verbosity
- Set `loglevel` to higher values (max 5) in `/etc/onvif.json`
- For foreground runs of `wsd_simple_server`, use `-d <level>`

## Common Build/Runtime Pitfalls
- Missing JSON keys: server uses defaults but will log warnings at higher verbosity
- Invalid JSON: check with `jq . /etc/onvif.json` and `jq . /etc/onvif.d/*.json`
- Permissions: ensure config and log destinations are writable by the service


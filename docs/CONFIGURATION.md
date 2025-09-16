# Configuration Guide (Modular JSON)

The ONVIF Simple Server uses a modular JSON configuration split between a main file and feature3pecific files.

- Main config: `/etc/onvif.json`
- Modular directory: `/etc/onvif.d/`
  - `profiles.json`  Media profiles and stream URLs
  - `ptz.json`  PTZ control and command hooks
  - `relays.json`  Relay outputs behavior and shell commands
  - `events.json`  Events support and filedriven inputs

The main config can reference an alternate directory via `conf_dir` (defaults to `/etc/onvif.d`).

## /etc/onvif.json (main)
```
{
  "model": "Model",
  "manufacturer": "Manufacturer",
  "firmware_ver": "0.0.1",
  "hardware_id": "HWID",
  "serial_num": "SN1234567890",
  "ifs": "wlan0",
  "port": 80,
  "user": "",
  "password": "",
  "loglevel": 3,                 // 0=FATAL..5=TRACE
  "conf_dir": "/etc/onvif.d"     // where to load modular JSON files from
}
```
Notes:
- `user`/`password` are optional. If set, server will expect digest auth in SOAP headers. RTSP URLs returned in Media services also embed credentials when present.
- `ifs` selects the network interface used for discovery and address resolution.

## /etc/onvif.d/profiles.json
An array of media profiles. The URL templates use `%s` placeholder for device IP/host.
```
[
  {
    "name": "Profile_0",
    "width": 1920,
    "height": 1080,
    "url": "rtsp://%s/ch0_0.h264",
    "snapurl": "http://%s/cgi-bin/snapshot.sh",
    "type": "H264",               // JPEG|MPEG4|H264
    "audio_encoder": "AAC",       // NONE|AAC|G711
    "audio_decoder": "G711"       // NONE|G711
  }
]
```

## /etc/onvif.d/ptz.json
```
{
  "enable": 1,
  "min_step_x": 0.0,
  "max_step_x": 360.0,
  "min_step_y": 0.0,
  "max_step_y": 180.0,
  "get_position": "/usr/local/bin/get_position",
  "is_moving": "/usr/local/bin/is_moving",
  "move_left": "/usr/local/bin/ptz_move -m left -s %f",
  "move_right": "/usr/local/bin/ptz_move -m right -s %f",
  "move_up": "/usr/local/bin/ptz_move -m up -s %f",
  "move_down": "/usr/local/bin/ptz_move -m down -s %f",
  "move_in": "/usr/local/bin/ptz_move -m in -s %f",
  "move_out": "/usr/local/bin/ptz_move -m out -s %f",
  "move_stop": "/usr/local/bin/ptz_move -m stop -t %s",
  "move_preset": "/usr/local/bin/ptz_move -p %d",
  "goto_home_position": "/usr/local/bin/ptz_move -h",
  "set_preset": "/usr/local/bin/ptz_presets.sh -a add_preset -n %d -m %s",
  "set_home_position": "/usr/local/bin/ptz_presets.sh -a set_home_position",
  "remove_preset": "/usr/local/bin/ptz_presets.sh -a del_preset -n %d",
  "jump_to_abs": "/usr/local/bin/ptz_move -j %f,%f,%f",
  "jump_to_rel": "/usr/local/bin/ptz_move -J %f,%f,%f",
  "get_presets": "/usr/local/bin/ptz_presets.sh -a get_presets"
}
```

## /etc/onvif.d/relays.json
```
[
  {
    "idle_state": "open",  // open|close
    "close": "/usr/local/bin/set_relay -n 0 -a close",
    "open":  "/usr/local/bin/set_relay -n 0 -a open"
  }
]
```

## /etc/onvif.d/events.json
```
{
  "enable": 3,  // 0=None, 1=PullPoint, 2=BaseSubscription, 3=Both
  "events": [
    {
      "topic": "tns1:VideoSource/MotionAlarm",
      "source_name": "Source",
      "source_type": "tt:ReferenceToken",
      "source_value": "VideoSourceToken",
      "input_file": "/tmp/onvif_notify_server/motion_alarm"
    }
  ]
}
```

## Using jct (JSON Config Tool)
The Thingino init scripts use `jct` to create/update JSON entries on boot.
Examples:
```
jct /etc/onvif.json create
jct /etc/onvif.json set conf_dir /etc/onvif.d
jct /etc/onvif.d/ptz.json create
jct /etc/onvif.d/ptz.json set enable 1
```

## Migration Notes
- The legacy monolithic `onvif_simple_server.json.example` has been removed.
- All keys are now split between the main file and the modular files as above.
- Paths are fixed under `/etc/onvif.json` and `/etc/onvif.d/` and can be adjusted by `conf_dir` only if needed.


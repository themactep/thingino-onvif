# API Overview

The ONVIF Simple Server implements multiple ONVIF services using SOAP 1.2 over HTTP. Responses are assembled from XML templates in the `*_service_files` directories.

Base URL endpoints (served as CGI):
- Device Service:        `/var/www/onvif/device_service`
- Media Service:         `/var/www/onvif/media_service`
- Media2 Service:        `/var/www/onvif/media2_service`
- PTZ Service:           `/var/www/onvif/ptz_service`
- Events Service:        `/var/www/onvif/events_service`
- DeviceIO Service:      `/var/www/onvif/deviceio_service`

The CGI handler is the same binary (`onvif_simple_server`) invoked under different script names. The service is chosen from the invoked program name.

## Device Service (subset)
- GetServiceCapabilities
- GetServices
- GetDeviceInformation
- GetSystemDateAndTime
- GetNetworkInterfaces
- GetUsers
- GetCapabilities (with/without PTZ)
- GetWsdlUrl
- SystemReboot

Templates: `device_service_files/*.xml`

## Media Service (subset)
- GetServiceCapabilities
- GetProfiles
- GetProfile
- GetStreamUri
- GetSnapshotUri
- GetCompatible*Configuration(s)
- Get*Configurations and Options (Video/Audio)

Templates: `media_service_files/*.xml`

## Media2 Service (subset)
- GetServiceCapabilities
- GetProfiles*
- GetStreamUri
- GetSnapshotUri
- Get*Configurations and Options (Video/Audio)

Templates: `media2_service_files/*.xml`

## PTZ Service (subset)
- GetServiceCapabilities
- GetNodes / GetNode
- GetConfigurations / GetConfiguration / GetConfigurationOptions
- GetPresets
- AbsoluteMove / RelativeMove / ContinuousMove / Stop
- GotoHomePosition / SetHomePosition / SetPreset

Templates: `ptz_service_files/*.xml`

## Events Service (subset)
- GetServiceCapabilities
- GetEventProperties
- SetSynchronizationPoint
- CreatePullPointSubscription
- PullMessages / Renew / Unsubscribe

Templates: `events_service_files/*.xml`

## DeviceIO Service (subset)
- GetServiceCapabilities
- GetAudioSources / GetAudioOutputs
- GetRelayOutputs / GetRelayOutputOptions
- SetRelayOutputState / SetRelayOutputSettings
- GetVideoSources

Templates: `deviceio_service_files/*.xml`

## Discovery (WS-Discovery)
The separate daemon `wsd_simple_server` implements WS-Discovery. It:
- Joins multicast group 239.255.255.250:3702
- Sends Hello/Bye messages
- Responds to Probe with ProbeMatches

Templates: `wsd_files/*.xml`

## Authentication
- If `user` and `password` are set in `/etc/onvif.json`, the server expects WS-Security UsernameToken with PasswordDigest.
- RTSP and snapshot URLs returned by Media/Media2 include embedded credentials if present.

## Notes
- Only a subset of ONVIF is implemented; unsupported operations respond with SOAP Fault.
- XML is assembled from templates with simple token substitution, see `utils.c:cat()`.


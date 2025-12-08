# Home Assistant WS-Discovery Compliance

## Overview

This document describes the changes made to ensure full compliance with Home Assistant's automatic ONVIF camera discovery mechanism.

## Home Assistant Discovery Requirements

Home Assistant's ONVIF integration (`homeassistant/components/onvif/config_flow.py`) uses WS-Discovery to automatically find ONVIF cameras on the network. The discovery process expects specific scopes in the WS-Discovery messages.

### Key Discovery Flow

1. **WS-Discovery Probe**: Home Assistant sends a multicast probe for devices of type `NetworkVideoTransmitter` with scope `onvif://www.onvif.org/Profile/Streaming`

2. **Parse ProbeMatches Response**: Extracts device information from scopes:
   - `onvif://www.onvif.org/name/{NAME}` - Device friendly name
   - `onvif://www.onvif.org/hardware/{HARDWARE}` - Hardware manufacturer
   - `onvif://www.onvif.org/mac/{MAC}` - **MAC address (used as unique device ID)**

3. **Fallback Mechanism**: If MAC is not in scopes:
   - Uses Endpoint Reference (EPR) as temporary name
   - Later attempts to get MAC via `GetNetworkInterfaces()` ONVIF call
   - Falls back to `SerialNumber` if network interfaces fail

## Critical Issue Identified

**The MAC address scope was missing from WS-Discovery messages.**

Without the MAC address in the scope, Home Assistant:
- Cannot immediately identify the device uniquely during discovery
- Must make additional ONVIF API calls to retrieve the MAC
- May experience slower or less reliable device setup

## Changes Made

### 1. Code Changes (`src/wsd_simple_server.c`)

#### Added MAC Address Variable
```c
char mac_address[18];  // Global variable to store MAC address
```

#### Retrieve MAC Address on Startup
```c
// Get MAC address for WS-Discovery scope
ret = get_mac_address(mac_address, if_name);
if (ret != 0) {
    log_warn("Unable to get MAC address, will be empty in WS-Discovery scopes");
    mac_address[0] = '\0';
} else {
    log_debug("MAC Address = %s", mac_address);
}
```

#### Updated All Message Templates
Updated parameter count from 12/14 to 14/16 and added MAC address substitution in:
- **Hello message** (sent on startup)
- **Bye message** (sent on shutdown)
- **ProbeMatches message** (sent in response to discovery probes)

### 2. XML Template Changes

#### `res/wsd_files/ProbeMatches.xml`
Added MAC address scope to the Scopes element:
```xml
<wsdd:Scopes>onvif://www.onvif.org/type/video_encoder 
             onvif://www.onvif.org/type/audio_encoder 
             onvif://www.onvif.org/type/ptz 
             onvif://www.onvif.org/hardware/%HARDWARE% 
             onvif://www.onvif.org/name/%NAME% 
             onvif://www.onvif.org/mac/%MAC_ADDRESS% 
             onvif://www.onvif.org/location/anywhere 
             onvif://www.onvif.org/Profile/Streaming</wsdd:Scopes>
```

#### `res/wsd_files/Hello.xml`
Added MAC address scope (without Profile/Streaming which is only for ProbeMatches):
```xml
<wsdd:Scopes>onvif://www.onvif.org/type/video_encoder 
             onvif://www.onvif.org/type/audio_encoder 
             onvif://www.onvif.org/type/ptz 
             onvif://www.onvif.org/hardware/%HARDWARE% 
             onvif://www.onvif.org/name/%NAME% 
             onvif://www.onvif.org/mac/%MAC_ADDRESS% 
             onvif://www.onvif.org/location/anywhere</wsdd:Scopes>
```

#### `res/wsd_files/Bye.xml`
Added MAC address scope for consistency.

## Verification

### Compliance Test

A new test script was created: `tests/test_wsd_ha_compliance.sh`

This script verifies:
1. ✅ Required WS-Discovery fields present
2. ✅ Required ONVIF scopes present (including MAC)
3. ✅ Correct SOAP structure
4. ✅ Template variable substitution

**Result**: All checks pass ✅

### Build Verification

The code compiles successfully with no errors or warnings:
```bash
./build.sh
# Build completed successfully!
```

## Benefits

1. **Faster Discovery**: Home Assistant can immediately identify the camera with its MAC address
2. **Reliable Unique ID**: No need for fallback mechanisms to retrieve MAC
3. **Better UX**: Smoother device setup experience in Home Assistant
4. **Standards Compliance**: Follows ONVIF WS-Discovery best practices

## Testing in Home Assistant

To test with Home Assistant:

1. Ensure the camera is on the same network as Home Assistant
2. In Home Assistant, go to: **Settings → Devices & Services → Add Integration → ONVIF**
3. Select **"Automatically discover ONVIF devices"**
4. The camera should appear with its proper name and be identified by MAC address
5. Check the device logs to verify MAC address is included in discovery scopes

## Reference

- Home Assistant ONVIF Integration: https://github.com/home-assistant/core/tree/dev/homeassistant/components/onvif
- Config Flow Discovery: `homeassistant/components/onvif/config_flow.py` lines 60-77
- ONVIF WS-Discovery Specification: Part II Section 3.1

## Related Files

- `src/wsd_simple_server.c` - WS-Discovery server implementation
- `res/wsd_files/ProbeMatches.xml` - ProbeMatches response template
- `res/wsd_files/Hello.xml` - Hello announcement template
- `res/wsd_files/Bye.xml` - Bye announcement template
- `tests/test_wsd_ha_compliance.sh` - Compliance verification script

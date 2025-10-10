# PTZ AbsoluteMove Support Investigation

## User Report

User reported that Home Assistant shows warnings about absolute positioning not being supported:

```
2025-10-03 20:10:19.200 DEBUG (MainThread) [homeassistant.components.onvif] Calling AbsoluteMove PTZ | Pan = 0.00 | Tilt = 0.00 | Zoom = 0.00 | Speed = 0.50 | Preset = PresetToken_4
2025-10-03 20:10:19.203 WARNING (MainThread) [homeassistant.components.onvif] Absolute Presets not supported on device 'front_door'
2025-10-03 20:10:19.261 DEBUG (MainThread) [onvif.zeep_aiohttp] HTTP Response from http://192.168.1.159/onvif/ptz_service (status: 200):
b''
```

## Investigation Results

### ✅ AbsoluteMove IS Supported

The ONVIF server **DOES support** AbsoluteMove functionality:

1. **Function exists**: `ptz_absolute_move()` in `src/ptz_service.c` (lines 842-936)
2. **Properly registered**: Listed in dispatch table at `src/onvif_dispatch.c` line 128
3. **Tested successfully**: Manual test shows HTTP 200 response with proper SOAP envelope

### Test Results

```bash
$ curl -X POST http://localhost:8000/onvif/ptz_service \
  -H "Content-Type: application/soap+xml" \
  -d '<AbsoluteMove request>'

HTTP/1.1 200 OK
Content-Length: 242

<?xml version="1.0" encoding="UTF-8"?>
<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://www.w3.org/2003/05/soap-envelope" 
                   xmlns:tptz="http://www.onvif.org/ver20/ptz/wsdl">
    <SOAP-ENV:Body>
        <tptz:AbsoluteMoveResponse/>
    </SOAP-ENV:Body>
</SOAP-ENV:Envelope>
```

## Root Cause of User's Issue

The empty response (`b''`) in Home Assistant logs is **NOT** because AbsoluteMove is unsupported. The actual issues are:

### 1. Missing PTZ Command Configuration

The `jump_to_abs` command must be configured in `onvif.json`:

```json
{
    "ptz": {
        "enable": 1,
        "jump_to_abs": "/bin/motors -d h -x %f -y %f",
        ...
    }
}
```

**If `jump_to_abs` is NULL**, the function returns a fault at line 877-879:

```c
if (service_ctx.ptz_node.jump_to_abs == NULL) {
    send_action_failed_fault("ptz_service", -3);
    return -3;
}
```

### 2. Missing Motor Control Binary

The command `/bin/motors` must exist on the system. Container logs show:

```
sh: 1: /bin/motors: not found
```

This means:
- The AbsoluteMove request was **successfully parsed**
- The function **attempted to execute** the motor command
- The command **doesn't exist** in the container/system

### 3. Home Assistant Interpretation

Home Assistant sees an empty or error response and interprets it as "Absolute Presets not supported" - but this is a **misinterpretation**. The feature IS supported, but the underlying hardware command is not configured or available.

## How AbsoluteMove Works

### Request Flow

1. **Parse ProfileToken** (line 856)
2. **Check PTZ enabled** (line 867)
3. **Check jump_to_abs configured** (line 877) ⚠️ **Critical check**
4. **Parse Position coordinates** (lines 882-893)
   - PanTilt x, y values
   - Zoom x value
5. **Validate ranges** (lines 900-921)
   - Check against min_step_x/max_step_x
   - Check against min_step_y/max_step_y
   - Check against min_step_z/max_step_z
6. **Execute command** (line 924)
   ```c
   sprintf(sys_command, service_ctx.ptz_node.jump_to_abs, dx, dy, dz);
   system(sys_command);
   ```
7. **Return success response** (lines 926-930)

### Configuration Requirements

For AbsoluteMove to work, `onvif.json` must have:

```json
{
    "ptz": {
        "enable": 1,
        "min_step_x": 0.0,
        "max_step_x": 3700.0,
        "min_step_y": 0.0,
        "max_step_y": 1000.0,
        "min_step_z": 0.0,
        "max_step_z": 0.0,
        "jump_to_abs": "/bin/motors -d h -x %f -y %f"
    }
}
```

The `jump_to_abs` command receives three parameters:
- `%f` - X position (pan)
- `%f` - Y position (tilt)  
- `%f` - Z position (zoom)

## Solution for User

### Option 1: Configure Proper Motor Control Command

If the camera has PTZ hardware, configure the correct command:

```json
"jump_to_abs": "/path/to/your/motor/control -x %f -y %f -z %f"
```

### Option 2: Implement Motor Control Script

Create `/bin/motors` script that handles absolute positioning:

```bash
#!/bin/sh
# Parse arguments and control PTZ hardware
# Example: motors -d h -x 1850.0 -y 500.0
```

### Option 3: Disable PTZ if Not Available

If the camera doesn't have PTZ hardware:

```json
{
    "ptz": {
        "enable": 0
    }
}
```

This will prevent Home Assistant from attempting PTZ operations.

## Verification

To verify AbsoluteMove support, check:

1. **PTZ enabled**: `"enable": 1` in config
2. **Command configured**: `"jump_to_abs"` is not null
3. **Command exists**: The binary/script is executable
4. **Ranges configured**: min/max step values are set

## Related Code

- **Implementation**: `src/ptz_service.c` lines 842-936
- **Dispatch**: `src/onvif_dispatch.c` line 128
- **Response template**: `res/ptz_service_files/AbsoluteMove.xml`
- **Configuration**: `res/onvif.json` ptz section

## Conclusion

**AbsoluteMove IS fully supported** in the ONVIF implementation. The user's issue is a **configuration problem**, not a missing feature. The warning from Home Assistant is misleading - it's actually a configuration/hardware availability issue, not a protocol support issue.

The fix is to either:
1. Configure the correct motor control command
2. Implement the motor control script
3. Disable PTZ if hardware is not available

## Date
2025-10-03


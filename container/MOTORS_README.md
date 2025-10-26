# Mock PTZ Motors Script

## Overview

The `motors` script is a mock PTZ (Pan-Tilt-Zoom) motor control application used in the container for testing ONVIF PTZ functionality. It simulates a real PTZ camera's motor control system with state tracking and detailed logging.

## Features

- **State Persistence**: Maintains current position (x, y, z) and movement status in `/tmp/motors_state`
- **Detailed Logging**: All commands and state changes are logged to stderr with `[MOTORS]` prefix
- **Full PTZ Support**: Implements all standard PTZ operations
- **Diagonal Movement**: Supports simultaneous multi-axis movement
- **Realistic Simulation**: Includes movement delays and status tracking

## Commands

### Get Current Position
```bash
motors -p
```
Returns current position in format: `x,y,z`

Example output:
```
2000.0,1000.0,1.0
```

### Get Movement Status
```bash
motors -b
```
Returns movement status:
- `0` = stopped
- `1` = moving

### Absolute Movement
```bash
motors -d h -x <x> -y <y> [-z <z>]
```
Moves to absolute position. Any axis can be omitted to keep current value.

Examples:
```bash
# Move to specific X and Y position
motors -d h -x 1850.0 -y 500.0

# Move only X axis
motors -d h -x 3000.0

# Move all three axes
motors -d h -x 2000.0 -y 1000.0 -z 2.0
```

### Relative Movement
```bash
motors -d g -x <dx> -y <dy> [-z <dz>]
```
Moves relative to current position.

Examples:
```bash
# Move right 100 units, up 50 units
motors -d g -x 100.0 -y 50.0

# Move left 200 units (negative value)
motors -d g -x -200.0
```

### Stop Movement
```bash
motors -d s
```
Stops all movement immediately.

### Go to Home Position
```bash
motors -d b
```
Returns to home position (center): x=2000.0, y=1000.0, z=1.0

## State File

The script maintains state in `/tmp/motors_state` with format:
```
x,y,z,moving
```

Example:
```
2000.0,1000.0,1.0,0
```

- **x**: Pan position (0-4000)
- **y**: Tilt position (0-2000)
- **z**: Zoom level (typically 1.0)
- **moving**: Movement status (0=stopped, 1=moving)

## Logging

All operations are logged to stderr with the `[MOTORS]` prefix. This allows the ONVIF server logs to show PTZ activity.

Example log output:
```
[MOTORS] Absolute move command: -d h -x 1850.0 -y 500.0
[MOTORS] Moving to absolute position: x=1850.0, y=500.0, z=1.0
[MOTORS] Position updated: x=1850.0, y=500.0, z=1.0, moving=1
```

## Integration with ONVIF

The script is configured in `/etc/onvif.json` for PTZ operations:

```json
{
  "ptz": {
    "enable": 1,
    "min_step_x": 0,
    "max_step_x": 4000,
    "min_step_y": 0,
    "max_step_y": 2000,
    "get_position": "/bin/motors -p",
    "is_moving": "/bin/motors -b",
    "move_left": "/bin/motors -d h -x 0",
    "move_right": "/bin/motors -d h -x 4000",
    "move_up": "/bin/motors -d h -y 0",
    "move_down": "/bin/motors -d h -y 2000",
    "move_stop": "/bin/motors -d s",
    "goto_home_position": "/bin/motors -d b",
    "jump_to_abs": "/bin/motors -d h -x %f -y %f",
    "jump_to_rel": "/bin/motors -d g -x %f -y %f"
  }
}
```

## Testing Diagonal Movement

The mock motors script fully supports diagonal movement, which is essential for testing the PTZ diagonal motion fix.

Example ONVIF ContinuousMove request with diagonal velocity:
```xml
<ContinuousMove>
  <ProfileToken>Profile_0</ProfileToken>
  <Velocity>
    <PanTilt x="-0.6666667" y="0.6666667"/>
  </Velocity>
</ContinuousMove>
```

This will trigger:
1. `motors -d h -x 0` (move left)
2. `motors -d h -y 0` (move up)

Both commands execute, enabling true diagonal movement.

## Viewing Logs

To see the motors script in action:

```bash
# View container logs
podman logs -f oss

# Look for [MOTORS] entries
podman logs oss 2>&1 | grep MOTORS

# Run tests with verbose output
./container_test.sh -v
```

## Movement Simulation

The script simulates realistic movement behavior:

1. **Immediate Response**: Commands execute immediately
2. **Moving Status**: Sets `moving=1` when movement starts
3. **Completion Delay**: After 0.1 seconds, sets `moving=0` (background process)
4. **State Persistence**: Position is maintained across calls

This allows testing of:
- Movement status queries during operation
- Position tracking
- Stop commands
- Sequential movements

## Troubleshooting

### State File Issues

If the state file becomes corrupted:
```bash
# Inside container
rm /tmp/motors_state
# Next motors command will recreate it
```

### Permission Issues

The script must be executable:
```bash
chmod +x /bin/motors
```

### Logging Not Visible

Ensure stderr is being captured in container logs. The Dockerfile and lighttpd configuration should preserve stderr output.

## Differences from Real Motors

This is a **mock** implementation for testing. Real PTZ motors would:

- Control actual hardware
- Have acceleration/deceleration curves
- Enforce physical limits
- Handle power management
- Report errors for mechanical issues

The mock script:
- Simulates state changes instantly
- Accepts any position values
- Never fails (except for invalid commands)
- Uses simple arithmetic for relative moves

## See Also

- `docs/PTZ_DIAGONAL_MOTION_FIX.md` - Diagonal motion implementation
- `container_test.sh` - Automated PTZ testing
- `tests/mock_motors.sh` - Alternative mock implementation for local testing


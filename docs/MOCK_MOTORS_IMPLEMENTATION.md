# Mock Motors Implementation for Container Testing

## Overview

A complete mock PTZ (Pan-Tilt-Zoom) motor control application has been implemented for the container environment. This enables comprehensive testing of ONVIF PTZ functionality, including the diagonal motion fix, without requiring actual PTZ hardware.

## Files Created/Modified

### New Files

1. **`container/motors`** - Mock motors executable
   - Full PTZ motor control simulation
   - State tracking and persistence
   - Detailed logging
   - 220 lines of shell script

2. **`container/MOTORS_README.md`** - Documentation
   - Complete usage guide
   - Command reference
   - Integration details
   - Troubleshooting tips

3. **`tests/test_mock_motors.sh`** - Test suite
   - 19 comprehensive tests
   - All PTZ operations covered
   - Diagonal movement verification
   - Automated validation

### Modified Files

1. **`container/Dockerfile`**
   - Added motors script installation
   - Made script executable
   - Placed in `/bin/motors` for ONVIF integration

2. **`container/onvif.json`** (already configured)
   - PTZ enabled with motors commands
   - All operations mapped to `/bin/motors`

## Features

### State Management

The mock motors maintains persistent state in `/tmp/motors_state`:

```
x,y,z,moving
```

Example: `2000.0,1000.0,1.0,0`

- **x**: Pan position (0-4000)
- **y**: Tilt position (0-2000)  
- **z**: Zoom level (typically 1.0)
- **moving**: Movement status (0=stopped, 1=moving)

### Supported Operations

| Operation | Command | Description |
|-----------|---------|-------------|
| Get Position | `motors -p` | Returns current x,y,z position |
| Get Status | `motors -b` | Returns moving status (0/1) |
| Absolute Move | `motors -d h -x X -y Y [-z Z]` | Move to absolute position |
| Relative Move | `motors -d g -x DX -y DY [-z DZ]` | Move relative to current |
| Stop | `motors -d s` | Stop all movement |
| Home | `motors -d b` | Return to home position |

### Logging

All operations log to stderr with `[MOTORS]` prefix:

```
[MOTORS] Absolute move command: -d h -x 1850.0 -y 500.0
[MOTORS] Moving to absolute position: x=1850.0, y=500.0, z=1.0
[MOTORS] Position updated: x=1850.0, y=500.0, z=1.0, moving=1
```

This integrates seamlessly with container logs for debugging.

## Integration with ONVIF

The motors script is fully integrated with the ONVIF PTZ service via `/etc/onvif.json`:

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

## Testing

### Unit Tests

Run the mock motors test suite:

```bash
./tests/test_mock_motors.sh
```

**Test Coverage:**
- ✓ Initial state verification
- ✓ Absolute movement (single and multi-axis)
- ✓ Relative movement (positive and negative)
- ✓ Home position
- ✓ Stop command
- ✓ Moving status tracking
- ✓ Diagonal movement simulation
- ✓ Logging output

**Results:** All 19 tests pass

### Container Integration Tests

The diagonal motion test in `container_test.sh` uses the mock motors:

```bash
./container_build.sh
./container_run.sh
./container_test.sh -v
```

Look for:
```
Testing: PTZ ContinuousMove with diagonal motion (x=-0.666667, y=0.666667)
✓ PTZ ContinuousMove (diagonal) - SUCCESS (both axes executed)
```

### Manual Testing

Inside the container:

```bash
# Enter container
podman exec -it oss /bin/bash

# Test commands
/bin/motors -p                          # Get position
/bin/motors -d h -x 1850.0 -y 500.0    # Absolute move
/bin/motors -d g -x 100.0 -y 50.0      # Relative move
/bin/motors -b                          # Check if moving
/bin/motors -d s                        # Stop
/bin/motors -d b                        # Go home
```

## Diagonal Movement Support

The mock motors fully supports diagonal movement, which is essential for testing the PTZ diagonal motion fix.

### Example Flow

1. **ONVIF Request:**
   ```xml
   <ContinuousMove>
     <ProfileToken>Profile_0</ProfileToken>
     <Velocity>
       <PanTilt x="-0.6666667" y="0.6666667"/>
     </Velocity>
   </ContinuousMove>
   ```

2. **ONVIF Server Executes:**
   ```bash
   /bin/motors -d h -x 0      # Move left
   /bin/motors -d h -y 0      # Move up
   ```

3. **Motors Logs:**
   ```
   [MOTORS] Absolute move command: -d h -x 0
   [MOTORS] Moving to absolute position: x=0, y=1000.0, z=1.0
   [MOTORS] Position updated: x=0, y=1000.0, z=1.0, moving=1
   [MOTORS] Absolute move command: -d h -y 0
   [MOTORS] Moving to absolute position: x=0, y=0, z=1.0
   [MOTORS] Position updated: x=0, y=0, z=1.0, moving=1
   ```

4. **Result:** Both axes move, achieving diagonal motion

## Viewing Logs

### Container Logs

```bash
# Follow all logs
podman logs -f oss

# Filter for motors activity
podman logs oss 2>&1 | grep MOTORS

# Last 20 lines
podman logs --tail 20 oss
```

### During Tests

```bash
# Run tests with verbose output
./container_test.sh -v

# Watch logs in another terminal
podman logs -f oss 2>&1 | grep -E "(MOTORS|PTZ)"
```

## Movement Simulation

The mock motors simulates realistic behavior:

1. **Command Received** → Logs command details
2. **State Updated** → Position changes immediately
3. **Moving Flag Set** → `moving=1`
4. **Background Process** → After 0.1s, sets `moving=0`

This allows testing:
- Sequential movements
- Status queries during movement
- Stop commands
- Position tracking

## Comparison with Real Motors

| Feature | Mock Motors | Real Motors |
|---------|-------------|-------------|
| State tracking | ✓ In memory | ✓ Hardware position |
| Movement delay | ✓ Simulated (0.1s) | ✓ Physical movement |
| Position limits | ✗ Accepts any value | ✓ Hardware limits |
| Error handling | ✗ Always succeeds | ✓ Mechanical errors |
| Power management | ✗ Not simulated | ✓ Required |
| Acceleration | ✗ Instant | ✓ Gradual |

The mock is designed for **functional testing**, not physical simulation.

## Troubleshooting

### State File Issues

```bash
# Inside container
rm /tmp/motors_state
# Next command recreates it
```

### Permissions

```bash
# Verify executable
ls -la /bin/motors
# Should show: -rwxr-xr-x
```

### Logging Not Visible

Check that stderr is captured in container logs. The Dockerfile and lighttpd configuration preserve stderr output.

### Position Not Updating

```bash
# Check state file
cat /tmp/motors_state

# Test manually
/bin/motors -d h -x 2000.0 -y 1000.0
/bin/motors -p
```

## Future Enhancements

Potential improvements for the mock motors:

1. **Configurable Limits** - Enforce min/max position values
2. **Speed Simulation** - Variable movement duration based on distance
3. **Error Injection** - Simulate hardware failures for testing
4. **Preset Storage** - Save/recall preset positions
5. **Tour Support** - Automated movement sequences
6. **Metrics** - Track movement statistics

## Related Documentation

- `container/MOTORS_README.md` - Detailed usage guide
- `docs/PTZ_DIAGONAL_MOTION_FIX.md` - Diagonal motion implementation
- `container_test.sh` - Integration tests
- `tests/test_mock_motors.sh` - Unit tests

## Summary

The mock motors implementation provides:

✓ **Complete PTZ simulation** for container testing  
✓ **State persistence** across commands  
✓ **Detailed logging** for debugging  
✓ **Full ONVIF integration** via configuration  
✓ **Diagonal movement support** for testing the fix  
✓ **Comprehensive test coverage** (19 unit tests)  
✓ **Easy debugging** with visible logs  

This enables thorough testing of PTZ functionality without hardware dependencies.


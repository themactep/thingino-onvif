# True Diagonal PTZ Motion Implementation

## Overview

This document describes the implementation of **true diagonal motion** for PTZ cameras, where both X and Y axes are controlled simultaneously with a single command, rather than two separate sequential commands.

## Problem

### Original Issue
PTZ ContinuousMove requests with diagonal motion (both X and Y velocities non-zero) were being simplified to single-axis motion due to early returns in the code.

### First Fix (Sequential Commands)
The initial fix removed early returns, allowing both X and Y commands to execute:
```bash
/bin/motors -d h -x 0    # Move left
/bin/motors -d h -y 0    # Move up
```

**Problem**: These are two separate commands executed sequentially, not true simultaneous diagonal movement.

### Final Solution (Single Diagonal Command)
The camera's motor controller can handle both axes in a single command for true simultaneous diagonal movement:
```bash
/bin/motors -d h -x 0.000000 -y 0.000000    # Move diagonally (left + up)
```

**Benefit**: True diagonal motion with both axes moving simultaneously.

## Implementation

### 1. Configuration Addition

Added new `continuous_move` configuration option in `res/onvif.json`:

```json
{
  "ptz": {
    "continuous_move": "/bin/motors -d h -x %f -y %f"
  }
}
```

This command format accepts both X and Y target positions in a single call.

### 2. Code Changes

#### Header File (`src/onvif_simple_server.h`)

Added new field to `ptz_node_t` struct:

```c
char *continuous_move;   // Continuous move with both axes: fmt(x_target, y_target) for diagonal movement
```

#### Configuration Loading (`src/conf.c`)

Added initialization, loading, and cleanup:

```c
// Initialize
service_ctx.ptz_node.continuous_move = NULL;

// Load from JSON
get_string_from_json(&(service_ctx.ptz_node.continuous_move), value, "continuous_move");

// Cleanup
if (service_ctx.ptz_node.continuous_move != NULL)
    free(service_ctx.ptz_node.continuous_move);
```

#### PTZ Service (`src/ptz_service.c`)

Modified `ptz_continuous_move()` function to detect diagonal movement and use single command:

```c
// Check if we have diagonal movement (both X and Y non-zero) and continuous_move command is configured
int has_diagonal = (x != NULL && dx != 0.0 && y != NULL && dy != 0.0);
int use_continuous_move = has_diagonal && (service_ctx.ptz_node.continuous_move != NULL);

if (use_continuous_move) {
    // Use single command for true diagonal movement
    double x_target, y_target;
    
    // Calculate target positions based on velocity direction
    if (dx > 0.0) {
        x_target = service_ctx.ptz_node.max_step_x;
    } else {
        x_target = service_ctx.ptz_node.min_step_x;
    }
    
    if (dy > 0.0) {
        y_target = service_ctx.ptz_node.min_step_y;  // Up means min
    } else {
        y_target = service_ctx.ptz_node.max_step_y;  // Down means max
    }
    
    sprintf(sys_command, service_ctx.ptz_node.continuous_move, x_target, y_target);
    log_debug("PTZ: Executing diagonal continuous_move command: %s", sys_command);
    system(sys_command);
    ret = 0;
} else {
    // Fall back to separate axis commands
    // ... existing code for single-axis movement ...
}
```

### 3. Logic Flow

1. **Parse velocities**: Extract X and Y velocity values from ONVIF request
2. **Detect diagonal**: Check if both X and Y velocities are non-zero
3. **Check configuration**: Verify `continuous_move` command is configured
4. **Calculate targets**: Determine target positions based on velocity directions
5. **Execute single command**: Send one command with both X and Y coordinates
6. **Fallback**: If not diagonal or `continuous_move` not configured, use separate commands

## Behavior

### Diagonal Movement (Both X and Y non-zero)

**ONVIF Request:**
```xml
<ContinuousMove>
  <ProfileToken>Profile_0</ProfileToken>
  <Velocity>
    <PanTilt x="-0.6666667" y="0.6666667"/>
  </Velocity>
</ContinuousMove>
```

**With `continuous_move` configured:**
```
[DEBUG] PTZ: ContinuousMove X velocity: -0.666667
[DEBUG] PTZ: ContinuousMove Y velocity: 0.666667
[DEBUG] PTZ: Executing diagonal continuous_move command: /bin/motors -d h -x 0.000000 -y 0.000000
[MOTORS] Absolute move command: -d h -x 0.000000 -y 0.000000
[MOTORS] Moving to absolute position: x=0.000000, y=0.000000, z=1.0
```

✅ **Single command with both axes**

**Without `continuous_move` configured:**
```
[DEBUG] PTZ: ContinuousMove X velocity: -0.666667
[DEBUG] PTZ: Executing move_left command: /bin/motors -d h -x 0
[DEBUG] PTZ: ContinuousMove Y velocity: 0.666667
[DEBUG] PTZ: Executing move_up command: /bin/motors -d h -y 0
```

✅ **Falls back to two separate commands**

### Single-Axis Movement (Only X or only Y)

Uses individual `move_left`, `move_right`, `move_up`, or `move_down` commands as before.

## Configuration

### Required Configuration

For true diagonal movement, add to `/etc/onvif.json`:

```json
{
  "ptz": {
    "enable": 1,
    "min_step_x": 0,
    "max_step_x": 4000,
    "min_step_y": 0,
    "max_step_y": 2000,
    "move_left": "/bin/motors -d h -x 0",
    "move_right": "/bin/motors -d h -x 4000",
    "move_up": "/bin/motors -d h -y 0",
    "move_down": "/bin/motors -d h -y 2000",
    "continuous_move": "/bin/motors -d h -x %f -y %f"
  }
}
```

### Optional Configuration

If `continuous_move` is not configured, the system automatically falls back to separate axis commands. This maintains backward compatibility with existing configurations.

## Testing

### Container Test

The `container_test.sh` includes a diagonal motion test:

```bash
./container_test.sh -v
```

**Expected output:**
```
Testing: PTZ ContinuousMove with diagonal motion (x=-0.666667, y=0.666667)
✓ PTZ ContinuousMove (diagonal) - SUCCESS (single command with both axes)
Recent logs showing diagonal command:
[MOTORS] Absolute move command: -d h -x 0.000000 -y 0.000000
```

### Manual Testing

```bash
# View logs in real-time
podman logs -f oss 2>&1 | grep MOTORS

# Send diagonal move request (use ONVIF client or test script)
# Watch for single command with both X and Y
```

## Benefits

### True Simultaneous Movement
- Both axes move at the same time
- Smoother diagonal motion
- More responsive camera control

### Hardware Efficiency
- Single command to motor controller
- Reduced command overhead
- Better synchronization between axes

### Backward Compatibility
- Automatic fallback to separate commands if `continuous_move` not configured
- Existing configurations continue to work
- No breaking changes

### ONVIF Compliance
- Proper implementation of ContinuousMove specification
- Correct handling of velocity vectors
- Expected behavior for diagonal motion

## Comparison

| Aspect | Sequential Commands | Single Diagonal Command |
|--------|-------------------|------------------------|
| Commands sent | 2 (X then Y) | 1 (X and Y together) |
| Execution | Sequential | Simultaneous |
| Smoothness | Good | Excellent |
| Synchronization | Approximate | Perfect |
| Configuration | Basic (4 commands) | Enhanced (5 commands) |
| Backward compatible | N/A | Yes (falls back) |

## Motor Controller Requirements

The motor controller must support the command format:
```bash
/bin/motors -d h -x <x_position> -y <y_position>
```

Where:
- `-d h`: Absolute positioning mode
- `-x <x_position>`: Target X coordinate
- `-y <y_position>`: Target Y coordinate

Both parameters must be accepted in a single command for true diagonal movement.

## Velocity to Position Mapping

The code maps ONVIF velocity directions to target positions:

| Velocity | Direction | Target Position |
|----------|-----------|----------------|
| dx > 0 | Right | max_step_x (4000) |
| dx < 0 | Left | min_step_x (0) |
| dy > 0 | Up | min_step_y (0) |
| dy < 0 | Down | max_step_y (2000) |

For diagonal movement, both X and Y targets are calculated and sent in one command.

## Files Modified

1. **src/onvif_simple_server.h** - Added `continuous_move` field
2. **src/conf.c** - Added configuration loading and cleanup
3. **src/ptz_service.c** - Added diagonal detection and single command execution
4. **res/onvif.json** - Added `continuous_move` configuration
5. **container_test.sh** - Updated test to verify single diagonal command

## See Also

- `docs/PTZ_DIAGONAL_MOTION_FIX.md` - Original diagonal motion fix
- `docs/MOCK_MOTORS_IMPLEMENTATION.md` - Mock motors for testing
- `container/MOTORS_README.md` - Motors command reference
- `DIAGONAL_MOTION_COMPLETE.md` - Complete implementation summary


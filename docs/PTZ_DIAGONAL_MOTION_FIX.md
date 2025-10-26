# PTZ Diagonal Motion Fix

## Problem

PTZ ContinuousMove requests with diagonal motion (both X and Y velocities non-zero) were being simplified to either horizontal or vertical motion only. The second axis was never processed.

### Example Request
```xml
<ContinuousMove xmlns="http://www.onvif.org/ver20/ptz/wsdl">
  <ProfileToken>Profile_0</ProfileToken>
  <Velocity>
    <PanTilt xmlns="http://www.onvif.org/ver10/schema" x="-0.6666667" y="0.6666667"/>
  </Velocity>
</ContinuousMove>
```

### Observed Behavior (Before Fix)
```
Oct 26 17:38:47 onvif_simple_server[25148]: [DEBUG:ptz_service.c:871]: PTZ: ContinuousMove X velocity: -0.666667
Oct 26 17:38:47 onvif_simple_server[25148]: [DEBUG:ptz_service.c:888]: PTZ: Executing move_left command: /bin/motors -d h -x 0
```

Only the X-axis (horizontal) movement was executed. The Y-axis (vertical) movement was never processed.

## Root Cause

The code in `ptz_continuous_move()` was processing X and Y velocities sequentially with early returns:

1. Process X velocity → execute move command → **return early**
2. Y velocity processing was never reached

The early returns happened at lines 876, 885, 901, and 910 in the original code.

## Solution

Restructured the velocity processing logic in `src/ptz_service.c` (lines 869-926):

### Changes Made

1. **Parse both velocities first** - Extract X and Y values before executing any commands
2. **Validate all commands upfront** - Check that required handlers exist before execution
3. **Execute both commands** - Remove early returns so both axes can be processed

### Code Structure (After Fix)

```c
// 1. Parse velocities first
if (x != NULL) {
    dx = atof(x);
    log_debug("PTZ: ContinuousMove X velocity: %f", dx);
}

if (y != NULL) {
    dy = atof(y);
    log_debug("PTZ: ContinuousMove Y velocity: %f", dy);
}

// 2. Validate all commands before executing
if (x != NULL && dx != 0.0) {
    if (dx > 0.0 && service_ctx.ptz_node.move_right == NULL) {
        send_action_failed_fault("ptz_service", -3);
        return -3;
    }
    // ... similar checks for other directions
}

// 3. Execute both commands (no early returns)
if (x != NULL && dx > 0.0) {
    sprintf(sys_command, service_ctx.ptz_node.move_right, dx);
    log_debug("PTZ: Executing move_right command: %s", sys_command);
    system(sys_command);
    ret = 0;
} else if (x != NULL && dx < 0.0) {
    sprintf(sys_command, service_ctx.ptz_node.move_left, -dx);
    log_debug("PTZ: Executing move_left command: %s", sys_command);
    system(sys_command);
    ret = 0;
}

if (y != NULL && dy > 0.0) {
    sprintf(sys_command, service_ctx.ptz_node.move_up, dy);
    log_debug("PTZ: Executing move_up command: %s", sys_command);
    system(sys_command);
    ret = 0;
} else if (y != NULL && dy < 0.0) {
    sprintf(sys_command, service_ctx.ptz_node.move_down, -dy);
    log_debug("PTZ: Executing move_down command: %s", sys_command);
    system(sys_command);
    ret = 0;
}
```

## Expected Behavior (After Fix)

For the same diagonal motion request (x=-0.666667, y=0.666667):

```
[DEBUG:ptz_service.c:872]: PTZ: ContinuousMove X velocity: -0.666667
[DEBUG:ptz_service.c:877]: PTZ: ContinuousMove Y velocity: 0.666667
[DEBUG:ptz_service.c:911]: PTZ: Executing move_left command: /bin/motors -d h -x 0
[DEBUG:ptz_service.c:918]: PTZ: Executing move_up command: /bin/motors -d h -y 0
```

Both axes are now processed, enabling true diagonal movement.

## Testing

### Automated Test

Added `test_ptz_continuous_move_diagonal()` to `container_test.sh`:

- Sends ContinuousMove request with diagonal velocity (x=-0.666667, y=0.666667)
- Verifies HTTP 200 response with ContinuousMoveResponse
- Checks container logs to confirm both X-axis (`-d h -x 0`) and Y-axis (`-d h -y 0`) commands were executed

### Manual Testing

```bash
# Build and run container
./container_build.sh
./container_run.sh

# Run tests
./container_test.sh -v

# Look for the diagonal motion test output:
# ✓ PTZ ContinuousMove (diagonal) - SUCCESS (both axes executed)
```

## Impact

### Benefits
- **Diagonal movement now works correctly** - PTZ cameras can move diagonally as intended
- **No breaking changes** - Existing single-axis movements continue to work
- **Better error handling** - All validations happen before any commands execute
- **Improved code structure** - Clearer separation of parsing, validation, and execution

### Compatibility
- Fully backward compatible with existing configurations
- Works with standard ONVIF PTZ motor commands
- No changes required to configuration files

## Related Files

- `src/ptz_service.c` - Main fix implementation
- `container_test.sh` - Added diagonal motion test
- `docs/PTZ_DIAGONAL_MOTION_FIX.md` - This documentation

## References

- ONVIF PTZ Service Specification v23.06
- Original issue: Diagonal PTZ requests simplified to single-axis motion
- Test request example from user logs (Oct 26 17:38:47)


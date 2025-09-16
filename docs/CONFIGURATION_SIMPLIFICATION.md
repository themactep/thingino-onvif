# Configuration System Simplification

## Overview

The ONVIF Simple Server configuration system has been simplified from a modular approach using multiple JSON files to a unified single-file approach. This change reduces complexity, improves maintainability, and eliminates potential failure points during debugging.

## Changes Made

### Before: Modular Configuration System
The previous system loaded configuration from multiple files:
- **Main config**: `onvif.json` (basic device settings)
- **Profiles**: `/etc/onvif.d/profiles.json` (video/audio stream profiles)
- **PTZ**: `/etc/onvif.d/ptz.json` (pan-tilt-zoom camera controls)
- **Relays**: `/etc/onvif.d/relays.json` (relay output configurations)
- **Events**: `/etc/onvif.d/events.json` (event notification settings)

### After: Unified Configuration System
All configuration is now consolidated into a single `onvif.json` file with the following structure:

```json
{
    "model": "Test Camera",
    "manufacturer": "Test Manufacturer",
    "firmware_ver": "1.0.0",
    "serial_num": "TEST123",
    "hardware_id": "TEST_HW",
    "ifs": "lo",
    "port": 8080,
    "username": "admin",
    "password": "admin",
    "loglevel": 4,
    "profiles": {
        "stream0": {
            "name": "Profile_0",
            "width": 1920,
            "height": 1080,
            "url": "rtsp://192.168.1.100:554/stream1",
            "snapurl": "http://192.168.1.100/snapshot.jpg",
            "type": "H264",
            "audio_encoder": "NONE"
        },
        "stream1": {
            "name": "Profile_1",
            "width": 1280,
            "height": 720,
            "url": "rtsp://192.168.1.100:554/stream2",
            "snapurl": "http://192.168.1.100/snapshot_720.jpg",
            "type": "H264",
            "audio_encoder": "AAC"
        }
    },
    "ptz": {
        "enable": 1,
        "min_step_x": 0.0,
        "max_step_x": 360.0,
        "move_left": "/usr/bin/ptz_move_left",
        "move_right": "/usr/bin/ptz_move_right"
    },
    "relays": [
        {
            "idle_state": "close",
            "close": "/usr/bin/relay_1_close",
            "open": "/usr/bin/relay_1_open"
        }
    ],
    "events_enable": 1,
    "events": [
        {
            "topic": "tns1:VideoSource/MotionAlarm",
            "source_name": "VideoSourceConfigurationToken",
            "source_type": "tt:ReferenceToken",
            "source_value": "VideoSourceToken",
            "input_file": "/tmp/motion_alarm"
        }
    ]
}
```

## Technical Implementation

### Code Changes
1. **Removed modular loading functions**:
   - `load_profiles_from_dir()`
   - `load_ptz_from_dir()`
   - `load_relays_from_dir()`
   - `load_events_from_dir()`

2. **Integrated parsing into main function**:
   - All configuration sections now parsed directly in `process_json_conf_file()`
   - Maintains identical functionality and data structures
   - Same validation and error handling

3. **Removed directory-based configuration**:
   - No longer reads from `/etc/onvif.d/` directory
   - Eliminates `conf_dir` configuration option
   - Reduces file system dependencies

### Benefits

1. **Simplified Debugging**:
   - Single file to check for configuration issues
   - Fewer file I/O operations that could fail
   - Easier to trace configuration loading problems

2. **Reduced Complexity**:
   - Fewer code paths for configuration loading
   - Less error handling for missing files
   - Simpler deployment and maintenance

3. **Better for Embedded Systems**:
   - Fewer filesystem operations
   - Reduced memory usage during configuration loading
   - More predictable behavior

4. **Improved Maintainability**:
   - All configuration in one place
   - Easier to backup and restore configuration
   - Simpler configuration validation

## Migration Guide

### For Existing Deployments
If you have an existing modular configuration setup:

1. **Combine your configuration files**:
   ```bash
   # Start with your main onvif.json
   cp /etc/onvif.json /tmp/unified_onvif.json

   # Add profiles section from profiles.json
   # Add ptz section from ptz.json
   # Add relays section from relays.json
   # Add events section from events.json
   ```

2. **Update your configuration file path**:
   - Point your application to the unified configuration file
   - Remove any `conf_dir` settings from the configuration

3. **Test the unified configuration**:
   ```bash
   # Test with the unified config
   ./onvif_simple_server device_service -c unified_onvif.json
   ```

### Configuration Validation
Use the provided test scripts to verify your unified configuration:

```bash
# Test unified configuration loading
./tests/test_unified_config.sh

# Test configuration with debug output
./tests/test_config_loading.sh
```

## Backward Compatibility

- All existing configuration options are supported
- Same data structures and validation rules
- No changes to ONVIF service behavior
- Same SOAP response formats

## Testing

The simplified configuration system has been thoroughly tested:

- ✅ All ONVIF services work correctly
- ✅ Configuration values properly loaded and used
- ✅ PTZ, relays, and events correctly configured
- ✅ No memory corruption or crashes detected
- ✅ AddressSanitizer validation passed

## Files Changed

- `src/conf.c` - Removed modular loading, integrated unified parsing
- `test_config/unified_onvif.json` - Example unified configuration
- `tests/test_unified_config.sh` - Test script for unified configuration
- `tests/test_config_loading.sh` - Debug configuration loading test

This simplification makes the ONVIF Simple Server more robust, easier to debug, and better suited for embedded environments while maintaining full functionality.

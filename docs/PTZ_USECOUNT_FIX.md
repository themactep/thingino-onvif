# PTZ UseCount Fix - Eliminating Duplicate PTZ Profile Entries

## Problem Summary

The ONVIF camera integration was reporting PTZ configurations with incorrect `UseCount` values, which could cause Home Assistant and other ONVIF clients to misinterpret the PTZ capabilities as duplicated or improperly shared.

## Root Cause

When the camera reported two media profiles (Profile_0 and Profile_1), both profiles included identical PTZ configurations with the same token (`PTZCfgToken`), but the `UseCount` was set to `0` instead of `2`. This violated the ONVIF specification which states that `UseCount` should reflect the number of profiles using a configuration.

### Before the Fix

```xml
<trt:Profiles token="Profile_0" fixed="true">
  ...
  <tt:PTZConfiguration token="PTZCfgToken" MoveRamp="0" PresetRamp="0" PresetTourRamp="0">
    <tt:Name>PTZCfg</tt:Name>
    <tt:UseCount>0</tt:UseCount>  <!-- INCORRECT -->
    <tt:NodeToken>PTZNodeToken</tt:NodeToken>
    ...
  </tt:PTZConfiguration>
</trt:Profiles>
<trt:Profiles token="Profile_1" fixed="true">
  ...
  <tt:PTZConfiguration token="PTZCfgToken" MoveRamp="0" PresetRamp="0" PresetTourRamp="0">
    <tt:Name>PTZCfg</tt:Name>
    <tt:UseCount>0</tt:UseCount>  <!-- INCORRECT -->
    <tt:NodeToken>PTZNodeToken</tt:NodeToken>
    ...
  </tt:PTZConfiguration>
</trt:Profiles>
```

### After the Fix

```xml
<trt:Profiles token="Profile_0" fixed="true">
  ...
  <tt:PTZConfiguration token="PTZCfgToken" MoveRamp="0" PresetRamp="0" PresetTourRamp="0">
    <tt:Name>PTZCfg</tt:Name>
    <tt:UseCount>2</tt:UseCount>  <!-- CORRECT - indicates 2 profiles share this config -->
    <tt:NodeToken>PTZNodeToken</tt:NodeToken>
    ...
  </tt:PTZConfiguration>
</trt:Profiles>
<trt:Profiles token="Profile_1" fixed="true">
  ...
  <tt:PTZConfiguration token="PTZCfgToken" MoveRamp="0" PresetRamp="0" PresetTourRamp="0">
    <tt:Name>PTZCfg</tt:Name>
    <tt:UseCount>2</tt:UseCount>  <!-- CORRECT - indicates 2 profiles share this config -->
    <tt:NodeToken>PTZNodeToken</tt:NodeToken>
    ...
  </tt:PTZConfiguration>
</trt:Profiles>
```

## Implementation Details

### Files Modified

1. **res/media_service_files/GetProfile_PTZ.xml**
   - Changed `<tt:UseCount>0</tt:UseCount>` to `<tt:UseCount>%USE_COUNT%</tt:UseCount>`
   - Added placeholder for dynamic UseCount value

2. **src/media_service.c**
   - Updated `media_get_profiles()` function
   - Added `%USE_COUNT%` parameter to all PTZ configuration calls
   - Single profile case: UseCount = 1
   - Two profile case: UseCount = 2 (for both profiles)

### Code Changes

#### Template File (res/media_service_files/GetProfile_PTZ.xml)
```xml
<tt:PTZConfiguration token="PTZCfgToken" MoveRamp="0" PresetRamp="0" PresetTourRamp="0">
    <tt:Name>PTZCfg</tt:Name>
    <tt:UseCount>%USE_COUNT%</tt:UseCount>  <!-- Changed from hardcoded 0 -->
    <tt:NodeToken>PTZNodeToken</tt:NodeToken>
    ...
</tt:PTZConfiguration>
```

#### Source Code (src/media_service.c)

**Single Profile Case (lines 259-277):**
```c
if (service_ctx.ptz_node.enable == 1) {
    size += cat(dest,
                "media_service_files/GetProfile_PTZ.xml",
                14,              // Changed from 12 to 14 (added 2 params)
                "%USE_COUNT%",   // Added parameter
                "1",             // UseCount = 1 for single profile
                "%MIN_X%",
                min_x,
                // ... rest of parameters
```

**Two Profile Case - Profile_0 (lines 330-348):**
```c
if (service_ctx.ptz_node.enable == 1) {
    size += cat(dest,
                "media_service_files/GetProfile_PTZ.xml",
                14,              // Changed from 12 to 14
                "%USE_COUNT%",   // Added parameter
                "2",             // UseCount = 2 (shared between profiles)
                "%MIN_X%",
                min_x,
                // ... rest of parameters
```

**Two Profile Case - Profile_1 (lines 388-406):**
```c
if (service_ctx.ptz_node.enable == 1) {
    size += cat(dest,
                "media_service_files/GetProfile_PTZ.xml",
                14,              // Changed from 12 to 14
                "%USE_COUNT%",   // Added parameter
                "2",             // UseCount = 2 (shared between profiles)
                "%MIN_X%",
                min_x,
                // ... rest of parameters
```

## ONVIF Specification Compliance

According to the ONVIF Media Service Specification:

> **UseCount**: Number of internal references currently using this configuration.
> When a configuration is shared across multiple profiles, the UseCount should reflect
> the total number of profiles using that configuration.

Our fix ensures:
- ✅ Single profile with PTZ: UseCount = 1
- ✅ Two profiles sharing PTZ: UseCount = 2 (for both)
- ✅ Same PTZ token indicates shared configuration
- ✅ Proper indication that both profiles use the same physical PTZ hardware

## Testing

The fix was tested using:
1. Container build and deployment
2. ONVIF GetProfiles request with authentication
3. XML response validation

### Test Results
- ✅ Build successful
- ✅ Container runs without errors
- ✅ GetProfiles returns correct UseCount values
- ✅ Both profiles show UseCount=2 when PTZ is enabled
- ✅ PTZ configuration properly shared between profiles

## Benefits

1. **Eliminates confusion** about duplicate PTZ entries
2. **ONVIF compliant** - follows specification correctly
3. **Better Home Assistant integration** - clients can properly understand shared configurations
4. **Accurate resource tracking** - UseCount reflects actual usage
5. **Maintains backward compatibility** - same PTZ token indicates shared config

## Related Files

- `res/media_service_files/GetProfile_PTZ.xml` - PTZ configuration template
- `src/media_service.c` - Media service implementation
- `xml/20251003_102006_response.xml` - Original response (before fix)
- `xml/new_response_with_ptz.xml` - New response (after fix)

## Date
2025-10-03


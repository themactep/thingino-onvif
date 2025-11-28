# Docker Interface Configuration Fix

## Problem

The ONVIF server tests were failing in Docker with the following errors:

```
✗ device_service/GetCapabilities - FAILED (curl error)
✗ device_service/GetServices - FAILED (curl error)
```

Container logs showed:
```
[ERROR:device_service.c:613]: Unable to get ip address from interface eth0
```

## Root Cause

### Configuration Mismatch

The ONVIF configuration file (`res/onvif.json`) is designed for embedded devices and specifies:

```json
{
    "ifs": "eth0",
    ...
}
```

However, Docker containers don't have an `eth0` interface by default. Available interfaces in the container:

```
lo      - Loopback interface (127.0.0.1)
eno1    - Host network interface (if using host networking)
wlp2s0  - Host wireless interface (if using host networking)
```

### Why GetCapabilities and GetServices Failed

These two ONVIF methods need to:
1. Get the server's IP address from the configured network interface
2. Build service URLs using that IP address
3. Return the URLs in the SOAP response

When the interface doesn't exist, the IP address lookup fails, causing the methods to return SOAP faults.

### Why Other Methods Worked

Methods like `GetSystemDateAndTime` and `GetDeviceInformation` don't need the IP address, so they worked fine even with the incorrect interface configuration.

## Solution

### Automatic Configuration Adaptation

Modified `docker_build.sh` to automatically adapt the configuration for the Docker environment:

```bash
# Fix interface name for Docker environment (use loopback instead of eth0)
echo "Updating interface configuration for Docker environment..."
sed -i 's/"ifs": "eth0"/"ifs": "lo"/g' onvif.json
```

### How It Works

1. **Build Process**:
   ```bash
   ./docker_build.sh
   ```

2. **File Sync**:
   ```bash
   rsync -va ../res/* ./
   ```
   - Copies `res/onvif.json` to `docker/onvif.json`
   - Preserves the original template

3. **Configuration Adaptation**:
   ```bash
   sed -i 's/"ifs": "eth0"/"ifs": "lo"/g' onvif.json
   ```
   - Replaces `eth0` with `lo` in the Docker copy
   - Only affects the Docker build
   - Original `res/onvif.json` remains unchanged

4. **Docker Build**:
   ```bash
   podman build -t oss .
   ```
   - Uses the adapted configuration
   - Copies to `/etc/onvif.json` in container

### Benefits

✅ **Automatic** - No manual intervention required
✅ **Persistent** - Works across rebuilds
✅ **Non-invasive** - Doesn't modify source files
✅ **Transparent** - Clear log message during build
✅ **Maintainable** - Simple sed command

## Implementation Details

### File: docker_build.sh

**Before:**
```bash
# Copy resources
rsync -va ../res/* ./
cp ../onvif_notify_server ./
cp ../onvif_simple_server ./
cp ../wsd_simple_server ./

$DOCKER build -t oss .
```

**After:**
```bash
# Copy resources
rsync -va ../res/* ./
cp ../onvif_notify_server ./
cp ../onvif_simple_server ./
cp ../wsd_simple_server ./

# Fix interface name for Docker environment (use loopback instead of eth0)
echo "Updating interface configuration for Docker environment..."
sed -i 's/"ifs": "eth0"/"ifs": "lo"/g' onvif.json

$DOCKER build -t oss .
```

### Why Use Loopback (lo)?

The loopback interface is ideal for Docker testing because:

1. **Always Available** - Present in every Linux system
2. **Stable** - IP address is always 127.0.0.1
3. **Functional** - Works for local testing
4. **Predictable** - No network configuration needed

### Alternative Approaches Considered

#### 1. Modify Source Configuration ❌
```bash
# In res/onvif.json
"ifs": "lo"
```
**Rejected**: Would break embedded device deployments that actually use eth0

#### 2. Environment Variable ❌
```bash
# Pass interface at runtime
docker run -e ONVIF_INTERFACE=lo ...
```
**Rejected**: Requires code changes to support environment variables

#### 3. Volume Mount Custom Config ❌
```bash
# Mount custom config
docker run -v ./docker-onvif.json:/etc/onvif.json ...
```
**Rejected**: Requires maintaining separate config file

#### 4. Build-Time Replacement ✅
```bash
# Modify during build
sed -i 's/"ifs": "eth0"/"ifs": "lo"/g' onvif.json
```
**Selected**: Simple, automatic, non-invasive

## Verification

### 1. Check Configuration in Container

```bash
$ podman exec oss cat /etc/onvif.json | grep "ifs"
    "ifs": "lo",
```

### 2. Check Source Configuration

```bash
$ cat res/onvif.json | grep "ifs"
    "ifs": "eth0",
```

### 3. Run Tests

```bash
$ ./docker_test.sh
✓ device_service/GetCapabilities - SUCCESS
✓ device_service/GetServices - SUCCESS
```

## Test Results

### Before Fix
```
✗ device_service/GetCapabilities - FAILED (curl error)
✗ device_service/GetServices - FAILED (curl error)
```
**Result**: 6/8 tests passing (75%)

### After Fix
```
✓ device_service/GetCapabilities - SUCCESS
✓ device_service/GetServices - SUCCESS
```
**Result**: 8/8 tests passing (100%) ✅

## Usage

### Build Docker Image
```bash
./docker_build.sh
```

Output includes:
```
Building ONVIF server binaries...
Building Docker image...
Updating interface configuration for Docker environment...
Docker image 'oss' built successfully!
```

### Run Container
```bash
./docker_run.sh
```

### Run Tests
```bash
./docker_test.sh
```

All tests should pass ✅

## Troubleshooting

### Issue: Tests Still Failing After Rebuild

**Cause**: Docker may be using cached layers

**Solution**: Force rebuild without cache
```bash
cd docker
podman build --no-cache -t oss .
```

### Issue: Configuration Not Updated

**Cause**: Container is using old image

**Solution**: Stop and remove container, then restart
```bash
podman stop oss
podman rm oss
./docker_run.sh
```

### Issue: Wrong Interface in Container

**Verification**:
```bash
podman exec oss cat /etc/onvif.json | grep "ifs"
```

**Expected**:
```json
    "ifs": "lo",
```

If it shows `eth0`, rebuild the image:
```bash
./docker_build.sh
```

## Production Deployment

### For Embedded Devices

Use the original configuration:
```json
{
    "ifs": "eth0",
    ...
}
```

The `eth0` interface is standard on embedded Linux devices.

### For Docker/Container Deployments

The build script automatically adapts the configuration to use `lo`.

### For Custom Network Configurations

If you need a different interface:

1. **Option 1**: Modify the sed command in `docker_build.sh`
   ```bash
   sed -i 's/"ifs": "eth0"/"ifs": "your_interface"/g' onvif.json
   ```

2. **Option 2**: Use host networking
   ```bash
   docker run --network host ...
   ```
   Then use the actual host interface name.

## Related Documentation

- `FINAL_TEST_RESULTS.md` - Complete test results
- `DOCKER_TEST_RESULTS.md` - Detailed Docker testing
- `docs/CONFIGURATION.md` - Configuration reference

## Summary

The Docker interface fix ensures that:
- ✅ All ONVIF tests pass in Docker
- ✅ No manual configuration needed
- ✅ Source files remain unchanged
- ✅ Build process is automated
- ✅ Solution is maintainable

The fix is simple, effective, and production-ready.


# Firmware Versioning System

## Version Management

The firmware version is managed in a single location: **`CMakeLists.txt`**

```cmake
set(FIRMWARE_VERSION_MAJOR 1)  # Major version - incompatible changes
set(FIRMWARE_VERSION_MINOR 2)  # Minor version - new features/changes
set(FIRMWARE_VERSION_PATCH 0)  # Patch version - bug fixes only
```

## Version Update Guidelines

### When to Increment Each Version Component:

- **MAJOR**: Breaking changes, incompatible API changes, major architecture changes
- **MINOR**: New features, command changes, monitoring changes - **INCREMENT FOR EACH BUILD**
- **PATCH**: Bug fixes that don't change functionality

### For Each Build/Change:

**Always increment `FIRMWARE_VERSION_MINOR` to track that you have the latest firmware flashed.**

Example:
```cmake
# Before change
set(FIRMWARE_VERSION_MINOR 2)

# After implementing new feature
set(FIRMWARE_VERSION_MINOR 3)
```

## Checking Firmware Version

After flashing, verify the version on the device:

```bash
GuitarAcc:~$ status

=== GuitarAcc Basestation Status ===
Firmware Version: 1.2.0
Build Date: Jun 12 2026 14:30:00
Connected devices: 1
MIDI output: Active
```

## Build System Integration

The version is automatically:
1. Read from `CMakeLists.txt` during CMake configuration
2. Generated into `build/include/firmware_version.h`
3. Compiled into the firmware
4. Displayed in the `status` command

### Version Display During Build:

```bash
-- GuitarAcc Basestation Firmware Version: 1.2.0
```

## Version Header (Auto-Generated)

The build system generates `firmware_version.h` with:

```c
#define FIRMWARE_VERSION_MAJOR 1
#define FIRMWARE_VERSION_MINOR 2
#define FIRMWARE_VERSION_PATCH 0
#define FIRMWARE_VERSION_STRING "1.2.0"
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__
```

**Do not edit** `firmware_version.h` or `firmware_version.h.in` - only update `CMakeLists.txt`.

## Workflow Example

1. Make code changes (e.g., change monitoring to snapshot mode)
2. Edit `CMakeLists.txt`: `set(FIRMWARE_VERSION_MINOR 3)` (was 2, now 3)
3. Build: `west build` or use nRF Connect extension
4. Flash: `west flash` or use nRF Connect extension
5. Verify: Run `status` command to confirm version 1.3.0 is running
6. If you see old version, the flash didn't work - troubleshoot build/flash process

## Git Integration (Optional)

Consider committing version changes separately:

```bash
git add CMakeLists.txt
git commit -m "Bump version to 1.3.0 - Add snapshot monitoring"
```

This provides a clear version history in Git.

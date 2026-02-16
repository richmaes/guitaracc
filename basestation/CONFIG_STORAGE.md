# Configuration Storage System

## Quick Reference: DEFAULT Area Write Protection

**Current Setting**: `CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=y` ✅ (Development mode)

**Storage Location**: Internal flash (last 16KB of 1MB)

### To Write Factory Defaults (Development):
```bash
# Option 1: UI Commands
config unlock_default
config write_default

# Option 2: Test Tool
./test_ui.py -w
```

### To Disable for Production:
```ini
# In prj.conf, change:
CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=n
```
Then rebuild. DEFAULT writes will be completely blocked at compile time.

---

## Overview

The GuitarAcc basestation uses **internal flash memory** on the nRF5340 for persistent configuration storage. The system implements a robust, redundant storage scheme that survives power cycles and protects against corruption.

**Note**: The nRF5340 Audio DK does not have external QSPI flash populated, so we use the internal 1MB flash instead (last 16KB reserved for config).

## Architecture

### Storage Areas

The internal flash is divided into three 4KB areas in the last 16KB of the 1MB flash:

1. **DEFAULT** (0xFC000-0xFCFFF): Factory default configuration
   - Written once during manufacturing/setup
   - Read-only during normal operation
   - Used for facFD000-0xFDFFF): Active storage area
   - Ping-pongs with AREA B
   - Contains current or previous configuration

3. **AREA B** (0xFE000-0xFEprevious configuration

3. **AREA B** (0x2000-0x2FFF): Active storage area
   - Ping-pongs with AREA A
   - Contains current or previous configuration

### Ping-Pong Algorithm

The system alternates between AREA A and AREA B when saving configuration:

```
Boot -> Read A & B -> Use highest sequence number -> Load config
Save -> Write to inactive area -> Increment sequence -> Update active
```

This approach provides:
- **Redundancy**: Two copies of configuration (current + previous)
- **Corruption protection**: If one write fails, previous config remains valid
- **Wear leveling**: Writes alternate between two areas

### Data Structure

Each storage area contains:

```c
struct config_header {
    uint32_t magic;      // 0x47544143 ("GTAC")
    uint32_t version;    // Config structure version
    uint32_t sequence;   // Sequence number (for ping-pong)
    uint32_t data_size;  // Size of config data
    uint8_t  hash[32];   // SHA256 hash of data
    uint32_t crc32;      // CRC32 of header
};

struct config_data {
    // MIDI configuration
    uint8_t midi_channel;
    uint8_t velocity_curve;
    uint8_t cc_mapping[6];
    
    // BLE configuration
    uint8_t max_guitars;
    uint8_t scan_interval_ms;
    
    // LED configuration
    uint8_t led_brightness;
    uint8_t led_mode;
    
    // Accelerometer configuration
    int16_t accel_deadzone;
    int16_t accel_scale[6];
    
    // Reserved space
    uint8_t reserved[128];
};
```

## Validation

Multiple levels of validation ensure data integrity:

1. **Magic Number**: Quick sanity check (0x47544143)
2. **Header CRC32**: Validates header structure
3. **SHA256 Hash**: Cryptographically validates configuration data
4. **Sequence Number**: Determines which area is most recent

## Boot Sequence

```
1. Initialize QSPI flash driver
2. Read headers from AREA A and AREA B
3. Validate both headers (magic, CRC)
4. Select area with highest valid sequence number
5. Validate data hash
6. If both invalid, try DEFAULT area
7. If DEFAULT invalid, use hardcoded defaults
8. Save valid config to active area if needed
```

## DEFAULT Area Write Protection

The system implements **double protection** to prevent accidental overwrites:

### 1. Compile-Time Protection
```c
// In Kconfig
CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=n  // Disabled by default
```

If disabled, all DEFAULT write attempts return `-EPERM` at compile time.

### 2. Runtime Lock
```c
// Must explicitly unlock before writing
config_storage_unlock_default_write();  // Unlocks for next write only
config_storage_write_default(&data);    // Succeeds and auto-locks
config_storage_write_default(&data);    // Fails - locked again
```

### 3. Status Check
```c
bool enabled = config_storage_is_default_write_enabled();
// Returns true only if:
// - CONFIG_CONFIG_ALLOW_DEFAULT_WRITE is enabled
// - Runtime lock is currently unlocked
```

### Development Workflow

**During Development:**
1. Build with `CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=y`
2. Use UI command: `config unlock_default` → `config write_default`
3. Or use test tool: `./test_ui.py -w`

**For Production:**
1. Change to `CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=n`
2. Rebuild firmware
3. DEFAULT area is now permanently protected

## API Usage

### Initialize (call once at boot)
```c
int err = config_storage_init();
if (err) {
    LOG_ERR("Config init failed: %d", err);
}
```

### Load Configuration
```c
struct config_data cfg;
int err = config_storage_load(&cfg);
if (err == 0) {
    // Use cfg.midi_channel, cfg.max_guitars, etc.
}
```

### Save Configuration
```c
struct config_data cfg;
config_storage_load(&cfg);

// Modify configuration
cfg.midi_channel = 5;
cfg.led_brightness = 200;

// Save to flash (writes to inactive area)
config_storage_save(&cfg);
```

### Restore Factory Defaults
```c
// Loads from DEFAULT area and saves to active storage
config_storage_restore_defaults();
```

### Get Storage Info (for debugging)
```c
enum config_area active_area;
uint32_t sequence;
config_storage_get_info(&active_area, &sequence);
LOG_INF("Active: %s, Seq: %u", 
        active_area == CONFIG_AREA_A ? "A" : "B", 
        sequence);
```

## UI Commands

The basestation provides a UART command interface (VCOM0 at 115200 baud):

### Show Configuration
```
GuitarAcc> config show
```
Displays all current configuration values.

### Save Configuration
```
GuitarAcc> config save
```
Saves current configuration to flash (ping-pongs to inactive area).

### Restore Factory Defaults
```
GuitarAcc> config restore
```
Loads configuration from DEFAULT area and saves to active storage.

### Write Factory Defaults (Manufacturing/Development Only!)

**Two-Step Process with Double Protection:**

```
GuitarAcc> config unlock_default
*** DEFAULT AREA UNLOCKED ***
You can now use 'config write_default'
Lock will auto-reset after write

GuitarAcc> config write_default
WARNING: Writing to factory default area!
Factory defaults written successfully
```

**Protection Mechanism:**
1. **Compile-time guard**: Must set `CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=y` in [prj.conf](prj.conf)
2. **Runtime lock**: Must call `unlock_default` immediately before `write_default`
3. **Auto-lock**: Lock automatically resets after write for safety

**For Production Builds:**
Set `CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=n` to completely disable DEFAULT writes at compile time.

## Test Application

The Python test tool supports writing factory defaults with the two-step unlock process:

```bash
# Interactive mode
./test_ui.py -p /dev/tty.usbmodem0010501849051

# Write factory defaults (with confirmation, auto-unlocks)
./test_ui.py -w -p /dev/tty.usbmodem0010501849051
```

The test tool automatically performs both steps:
1. Sends `config unlock_default` command
2. Sends `config write_default` command

## Development vs Production Configuration

### Development Build (prj.conf)
```ini
# Enable DEFAULT area writes for development/testing
CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=y
```

### Production Build (prj.conf)
```ini
# Disable DEFAULT area writes to prevent accidental overwrites
CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=n
```

**Recommendation**: Use a separate `prj_production.conf` file for production builds to ensure DEFAULT writes are always disabled.

## Adding New Configuration Parameters

To add new configuration parameters:

1. Update `struct config_data` in [config_storage.h](src/config_storage.h)
2. Update `config_storage_get_hardcoded_defaults()` in [config_storage.c](src/config_storage.c)
3. Update `cmd_config()` in [ui_interface.c](src/ui_interface.c) to display new parameters
4. Use the parameters in your application code

Example:
```c
// 1. Add to struct config_data
struct config_data {
    // ... existing fields ...
    uint8_t new_parameter;
};

// 2. Set default value
void config_storage_get_hardcoded_defaults(struct config_data *data) {
    // ... existing defaults ...
    data->new_parameter = 42;
}

// 3. Use in application
struct config_data cfg;
config_storage_load(&cfg);
use_parameter(cfg.new_parameter);
```

## Memory Layout

```
Internal Flash (nRF5340 - 1MB)
├── 0x00000-0xFBFFF: Code and data (~1008KB)
├── 0xFC000-0xFCFFF (4KB): DEFAULT area
├── 0xFD000-0xFDFFF (4KB): AREA A
├── 0xFE000-0xFEFFF (4KB): AREA B
└── 0xFF000-0xFFFFF (4KB): Reserved for future use
```

Total config storage: 16KB (1.6% of 1MB flash)

## Zephyr Configuration

Required Kconfig options in [prj.conf](prj.conf):

```ini
# Settings subsystem
CONFIG_SETTINGS=y
CONFIG_SETTINGS_NVS=y
CONFIG_NVS=y
Flash drivers
CONFIG_FLASH=y
CONFIG_FLASH_PAGE_LAYOUT=y
CONFIG_FLASH_MAP=y
CONFIG_MPU_ALLOW_FLASH_WRITE=y

# Crypto for hash validation
CONFIG_MBEDTLS=y
CONFIG_MBEDTLS_SHA256_C=y
```

No device tree overlay needed - uses internal flash controller automatically.  status = "okay";
};
```

## Advantages Over SD Card

| Feature | QSPI Flash | SD Card |
|---------|-----------|---------|
| **Speed** | Very fast (104MHz) | Slower (SPI mode) |
| **Reliability** | No moving parts | Mechanical contacts |
| **Power** | Low (~5mA) | Higher (~50-100mA) |
| **Size** | Built-in chip | External card |
| **Complexity** | Simple driver | Filesystem overhead |
| **Wear** | >100K cycles | ~10K cycles |
| **Corruption** | Hash validation | FS corruption risk |
| **Latency** | Microseconds | Milliseconds |

## Error Handling

The system gracefully handles various failure scenarios:

- **Flash driver not ready**: Falls back to hardcoded defaults
- **Corrupt header**: Tries other area, then DEFAULT
- **Hash mismatch**: Tries other area, then DEFAULT  
- **Both areas corrupt**: Loads from DEFAULT
- **All areas corrupt**: Uses hardcoded defaults
- **Write faInternal Flash | SD Card |
|---------|---------------|---------|
| **Speed** | Very fast | Slower (SPI mode) |
| **Reliability** | Solid state | Mechanical contacts |
| **Power** | Minimal | Higher (~50-100mA) |
| **Size** | Built-in | External card |
| **Complexity** | Simple driver | Filesystem overhead |
| **Wear** | >10K cycles per page | ~10K cycles |
| **Corruption** | Hash validation | FS corruption risk |
| **Latency** | Microseconds | Milliseconds |
| **Integration** | No additional hardware | Requires SD slot
Possible improvements:

- Compression for larger configurations
- Encryption for sensitive data
- Configuration history/rollback
- Remote configuration updates via BLE
- Configuration profiles (multiple saved configs)

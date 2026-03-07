# Basestation UI Interface

## Overview
The basestation provides a comprehensive command-line interface via **Zephyr Shell** over the USB VCOM port. This interface enables configuration management, system monitoring, and MIDI diagnostics.

## Implementation

### Zephyr Shell
The UI is now implemented using the **Zephyr Shell** subsystem, which provides:
- Advanced command parsing and tab completion
- Command history with up/down arrow navigation
- Hierarchical command structure
- Integrated logging output
- ANSI color support
- Built-in help system

### Hardware Configuration

**USB VCOM Port:**
- **Baud Rate**: 115200
- **Settings**: 8N1, no flow control
- **Port**: Typically `/dev/tty.usbmodem*` on macOS/Linux, `COMx` on Windows

## Current Features

### Command Structure

Commands are organized hierarchically using the Zephyr Shell:

#### Status Commands
- `status` - Display system status (connected devices, MIDI output state, config area)

#### Configuration Commands (`config` submenu)
- `config show` - Display all current configuration values
- `config save` - Save current configuration to flash
- `config restore` - Restore factory default configuration
- `config patch <0-15>` - Show specific patch configuration
- `config select <0-15>` - Select active patch
- `config list` - List all patches
- `config midi_ch <1-16>` - Set MIDI output channel
- `config cc <x|y|z> <0-127>` - Set CC number for each accelerometer axis
- `config accel_min <0-5> <0-127>` - Set minimum CC value for axis
- `config accel_max <0-5> <0-127>` - Set maximum CC value for axis
- `config accel_invert <0-5> <0|1>` - Enable/disable axis inversion
- `config velocity_curve <0-127>` - Set velocity curve for active patch
- `config scan_interval <10-1000>` - Set BLE scan interval in ms
- `config avg_enable <0|1>` - Enable/disable running average filter
- `config avg_depth <3-10>` - Set running average depth
- `config erase_all` - Erase all configuration (testing only)

#### Configuration Import/Export Commands (`config` submenu)
- `config export` - Export entire configuration (global + all patches) in JSON format
- `config export global` - Export only global configuration
- `config export patch <0-15>` - Export single patch configuration
- `config import` - Import configuration from JSON input (interactive mode)
  - Supports full, global-only, or single-patch updates
  - Validates input before applying changes
  - Automatically saves to flash after successful import

#### MIDI Commands (`midi` submenu)
- `midi rx_stats` - Show MIDI receive statistics
  - Total bytes received
  - Clock messages (0xF8) with BPM calculation
  - Start/Stop/Continue messages
  - Other real-time messages
- `midi rx_reset` - Reset MIDI receive statistics counters
- `midi program [0-127]` - Get or set current MIDI program number
- `midi send_rt <0xF8-0xFF>` - Send real-time MIDI message (Clock, Start, Stop, etc.)

### Shell Features
- **Tab Completion**: Press Tab to autocomplete commands and show available options
- **Command History**: Use Up/Down arrows to recall previous commands
- **Help System**: Type `help` or `<command> -h` for command information
- **Subcommands**: Commands organized in logical groups
- **Real-time Logging**: System logs displayed alongside command output
- **Color Support**: Commands use ANSI colors for better readability

## Code Implementation

### Module Structure
The UI interface is implemented using Zephyr Shell:
- `src/ui_interface.h` - Public API and data structures
- `src/ui_interface_shell.c` - Shell command implementations
- Integration with Zephyr Shell subsystem

### Public API
```c
/* Initialize the UI interface */
int ui_interface_init(void);

/* Update connected devices count */
void ui_set_connected_devices(int count);

/* Update MIDI output active state */
void ui_set_midi_output_active(bool active);

/* Get MIDI RX statistics */
void ui_get_midi_rx_stats(struct midi_rx_stats *stats);

/* Reset MIDI RX statistics */
void ui_reset_midi_rx_stats(void);

/* Get/Set current MIDI program */
uint8_t ui_get_current_program(void);
void ui_set_current_program(uint8_t program);

/* Send real-time MIDI message */
int send_midi_realtime(uint8_t rt_byte);

/* Configuration reload callback */
extern void (*ui_config_reload_callback)(void);
```

### MIDI Statistics Structure
```c
struct midi_rx_stats {
    uint32_t total_bytes;
    uint32_t clock_messages;      /* 0xF8 MIDI Timing Clock */
    uint32_t start_messages;      /* 0xFA MIDI Start */
    uint32_t continue_messages;   /* 0xFB MIDI Continue */
    uint32_t stop_messages;       /* 0xFC MIDI Stop */
    uint32_t other_messages;
    uint32_t last_clock_time;     /* Timestamp of last clock */
    uint32_t clock_interval_us;   /* Interval in microseconds */
};
```

### Initialization
The Zephyr Shell is automatically initialized by the Zephyr subsystem:
```c
/* In main() */
err = ui_interface_init();
if (err) {
    LOG_ERR("Failed to initialize UI interface (err %d)", err);
} else {
    LOG_INF("UI interface ready (Zephyr Shell)");
    /* Set config reload callback */
    ui_config_reload_callback = reload_config;
}
```

No UART setup is required - Zephyr Shell uses the console backend configured in the device tree.

## Accessing the Interface

### macOS/Linux Terminal
```bash
# Find the device
ls /dev/tty.usbmodem*

# Connect using screen
screen /dev/tty.usbmodem0010501849051 115200

# Or use minicom
minicom -D /dev/tty.usbmodem0010501849051 -b 115200

# Or use cu
cu -l /dev/tty.usbmodem0010501849051 -s 115200
```

### Python Test Script
A Python test tool is provided for testing and automation:

```bash
# Interactive mode (requires select module)
./basestation/test_ui.py -p /dev/tty.usbmodem0010501849051

# Automated test mode
./basestation/test_ui.py -t -p /dev/tty.usbmodem0010501849051

# List available ports
./basestation/test_ui.py -l

# Help
./basestation/test_ui.py -h
```

#### Python Script Features
- Interactive terminal mode
- Automated command testing
- Hardware flow control support
- Port discovery
- Error handling
- Factory default writer (`-w` option)

### Example Session
```
uart:~$ help
Please press the <Tab> button to see all available commands.
You can also use the <Tab> button to prompt or auto-complete all commands or its subcommands.
You can try to call commands with <-h> or <--help> parameter for more information.

uart:~$ status
Config area: A (seq=1)

=== GuitarAcc Basestation Status ===
Connected devices: 1
MIDI output: Active

uart:~$ config show

=== Configuration ===
MIDI:
  Channel: 1
  Velocity curve: 0
  CC mapping: [16, 17, 18, 19, 20, 21]
BLE:
  Max guitars: 4
  Scan interval: 100 ms
LED:
  Brightness: 128
  Mode: 0
Accelerometer:
  Deadzone: 100
  Scale: [1000, 1000, 1000, 1000, 1000, 1000]

uart:~$ midi rx_stats

=== MIDI RX Statistics ===
Total bytes received: 6162
Clock messages (0xF8): 6162
Clock interval: 28000 us (~89 BPM)
Start messages (0xFA): 0
Continue messages (0xFB): 0
Stop messages (0xFC): 0
Other messages: 0

uart:~$ midi program
Current MIDI Program: 1

uart:~$ midi program 5
MIDI Program set to 5

uart:~$ config midi_ch 2
MIDI channel set to 2

uart:~$ config cc x 74
X-axis CC set to 74
uart:~$ config save
Configuration saved to flash

uart:~$ 
```

### MIDI Monitoring

The shell provides comprehensive MIDI diagnostics:

**View Real-time Statistics:**
```
uart:~$ midi rx_stats
=== MIDI RX Statistics ===
Total bytes received: 12450
Clock messages (0xF8): 12450
Clock interval: 27777 us (~90 BPM)
Start messages (0xFA): 1
Continue messages (0xFB): 0
Stop messages (0xFC): 1
Other messages: 0
```

**Check Current Program:**
```
uart:~$ midi program
Current MIDI Program: 42
```

**Send Test Messages:**
```
uart:~$ midi send_rt 0xFA
Sent real-time message: 0xFA

uart:~$ midi send_rt 0xF8
Sent real-time message: 0xF8
```

### Runtime Configuration
The basestation includes a complete configuration management system with persistent storage in internal flash. See [CONFIG_STORAGE.md](CONFIG_STORAGE.md) for details.

**Features:**
- MIDI channel configuration (1-16)
- CC number mapping for each accelerometer axis
- Persistent storage with redundancy (ping-pong areas)
- SHA256 hash validation
- Factory default area with write protection
- Runtime reload (changes take effect immediately)

**Default Configuration:**
- MIDI Channel: 1
- X-axis: CC 16 (General Purpose Controller 1)
- Y-axis: CC 17 (General Purpose Controller 2)
- Z-axis: CC 18 (General Purpose Controller 3)
- Roll: CC 19, Pitch: CC 20, Yaw: CC 21

### Configuration Commands

**View Current Configuration:**
```
GuitarAcc> config show
```

**Set MIDI Channel:**
```
GuitarAcc> config midi_ch 5
MIDI channel set to 5
```

**Set CC Number for Axis:**
```
GuitarAcc> config cc x 74    # Set X-axis to CC 74 (Brightness)
X-axis CC set to 74

GuitarAcc> config cc y 1     # Set Y-axis to CC 1 (Modulation)
Y-axis CC set to 1

GuitarAcc> config cc z 11    # Set Z-axis to CC 11 (Expression)
Z-axis CC set to 11
```

**Save Configuration:**
```
GuitarAcc> config save
Configuration saved to flash
```
*Note: Configuration is auto-saved when you change settings*
CONFIG_STORAGE.md](CONFIG_STORAGE.md) - Configuration storage system details
- [
**Restore Factory Defaults:**
```
GuitarAcc> config restore
Factory defaults restored
```

**Write Factory Defaults (Development Only):**
```
GuitarAcc> config unlock_default
*** DEFAULT AREA UNLOCKED ***
You can now use 'config write_default'
Lock will auto-reset after write

GuitarAcc> config write_default
WARNING: Writing to factory default area!
Factory defaults written successfully
```
*Note: Requires `CONFIG_CONFIG_ALLOW_DEFAULT_WRITE=y` in build*

## Future Enhancements

### Planned Features
1. **Extended MIDI Diagnostics**
   - Message rate monitoring
   - Jitter analysis for MIDI clock
   - Full MIDI parser for all message types
   - MIDI thru control (enable/disable)

2. **Program-Based Features**
   - Mapping profiles per program number
   - Program-specific CC routing
   - Effect parameter control
   - Preset management

3. **Advanced Configuration**
   - Real-time accelerometer value display
   - MIDI activity monitoring
   - BLE connection statistics
   - Enhanced error reporting

## Configuration Import/Export

### Overview
The configuration import/export feature enables backup, sharing, and remote configuration of the basestation. Configuration data is exchanged in JSON format, which is human-readable, version-control friendly, and easily extensible.

### Export Format

The export format uses JSON with a hierarchical structure:

```json
{
  "version": 1,
  "config": {
    "global": {
      "default_patch": 0,
      "midi_channel": 0,
      "max_guitars": 4,
      "ble_scan_interval_ms": 100,
      "led_brightness": 128,
      "accel_scale": [1000, 1000, 1000, 1000, 1000, 1000],
      "running_average_enable": true,
      "running_average_depth": 5
    },
    "patches": [
      {
        "patch_num": 0,
        "patch_name": "Patch 0",
        "velocity_curve": 0,
        "cc_mapping": [10, 94, 4, 19, 20, 21],
        "led_mode": 0,
        "accel_deadzone": 100,
        "accel_min": [0, 0, 0, 0, 0, 0],
        "accel_max": [127, 127, 127, 127, 127, 127],
        "accel_invert": 0
      }
      ... (patches 1-15)
    ]
  }
}
```

### Export Commands

#### Export Full Configuration
```bash
config export
```
Outputs complete configuration including global settings and all 16 patches.

#### Export Global Settings Only
```bash
config export global
```
Outputs only the global configuration section.

#### Export Single Patch
```bash
config export patch 5
```
Exports configuration for patch 5 only.

### Import Format

The import command accepts JSON in the same format as export. Three types of imports are supported:

#### Full Configuration Import
```json
{
  "version": 1,
  "config": {
    "global": { ... },
    "patches": [ ... ]
  }
}
```
Updates both global settings and all patches.

#### Global-Only Import
```json
{
  "version": 1,
  "config": {
    "global": {
      "midi_channel": 5,
      "ble_scan_interval_ms": 200
    }
  }
}
```
Updates only global settings, leaves patches unchanged.

#### Single-Patch Import
```json
{
  "version": 1,
  "config": {
    "patches": [
      {
        "patch_num": 3,
        "patch_name": "Custom Patch",
        "cc_mapping": [20, 21, 22, 23, 24, 25]
      }
    ]
  }
}
```
Updates only the specified patch (patch 3), leaves other patches and global settings unchanged.

### Import Command

#### Interactive Import
```bash
config import
```

The device enters line-by-line input mode. Paste or type the JSON configuration, then send a terminating sequence to process.

**Validation:**
- JSON syntax is validated before parsing
- Field ranges are checked (e.g., MIDI channel 0-15, CC values 0-127)
- Invalid fields are rejected with error messages
- Configuration is only updated if all validations pass

**Auto-Save:**
After successful validation and import, the configuration is automatically saved to flash.

### Usage Workflows

#### Backup Configuration
```bash
# Export to file via serial capture
config export > basestation_config_backup.json
```

#### Share/Clone Configuration
```bash
# On source device
config export
# Copy output to file

# On target device
config import
# Paste JSON content
```

#### Update Single Patch Remotely
```bash
# Export just one patch as template
config export patch 0
# Edit the JSON, then import modified patch
config import
```

#### Batch Configuration via Script
Python scripts can automate configuration:
```python
import serial
import time

config_json = """
{
  "version": 1,
  "config": {
    "global": {
      "midi_channel": 3
    }
  }
}
"""

ser = serial.Serial('/dev/ttyUSB0', 115200)
ser.write(b'config import\r\n')
time.sleep(0.5)
ser.write(config_json.encode())
ser.write(b'\x04\r\n')  # Send terminator
```

### Format Extensibility

The JSON format is designed for extensibility:

**Version Field:**
- `"version": 1` identifies the schema version
- Future schema changes increment the version
- Older firmware can reject newer schemas gracefully

**Optional Fields:**
- Missing fields use current values (merge behavior)
- Extra fields are ignored (forward compatibility)
- Allows older config files to work with newer firmware

**Adding New Parameters:**
1. Add field to JSON schema documentation
2. Update export command to include new field
3. Update import validation to handle new field
4. Increment version if breaking changes

### Error Handling

Import errors are reported with specific messages:

- **Syntax Error:** `JSON parse error at line X`
- **Range Error:** `Field 'midi_channel' out of range (0-15)`
- **Invalid Type:** `Field 'running_average_enable' must be boolean`
- **Missing Required:** `Required field 'version' not found`

On error, the current configuration remains unchanged.

### Python Helper Tool

A Python tool (`config_tool.py`) will be provided for configuration management:

```bash
# Export config to file
./config_tool.py export -p /dev/ttyUSB0 -o config.json

# Import config from file
./config_tool.py import -p /dev/ttyUSB0 -i config.json

# Validate JSON without importing
./config_tool.py validate -i config.json
```

## Migration Notes

The UI system was migrated from a custom UART-based implementation to Zephyr Shell:

**Benefits:**
- Standard Zephyr subsystem (well-tested, maintained)
- Rich feature set (tab completion, history, colors)
- Easier to extend with new commands
- Better integration with logging
- No custom UART interrupt handling needed

**Breaking Changes:**
- Command prompt changed from `GuitarAcc>` to `uart:~$`
- Welcome banner removed (standard Zephyr boot log shown)
- Some command syntax may differ slightly

## Related Documentation
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture overview
- [MAPPING.md](MAPPING.md) - Accelerometer to MIDI mapping
- [REFACTORING.md](REFACTORING.md) - Code organization

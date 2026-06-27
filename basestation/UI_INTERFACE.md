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
- `monitor [json]` - Show real-time pipeline snapshot
  - Without arguments: human-readable format showing raw accelerometer data, rotated vectors, and MIDI output
  - With `json`: single-line JSON format for logging/scripting
    - Example: `{"timestamp_ms":12345,"raw_axis":{"x":100,"y":200,"z":300},"input_vector":{"x":0.100,"y":0.200,"z":0.300},"rotated_vector":{"x":0.150,"y":0.250,"z":0.350},"normalized_vector":{"x":0.316,"y":0.527,"z":0.738},"scalar_projection":0.316,"function_type":"linear","midi_output":{"cc":1,"value":64}}`

#### Configuration Commands (`config` submenu)
- `config show` - Display all current configuration values
- `config save` - Save current configuration to flash
- `config restore` - Restore factory default configuration
- `config reload` - Reload configuration from storage
- `config patch <0-15>` - Show specific patch configuration
- `config select <0-15>` - Select active patch
- `config list` - List all patches
- `config midi_ch <1-16>` - Set MIDI output channel
- `config accel_deadzone <0-127>` - Set CC change threshold
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

#### Topology Commands (`topo` submenu)
- `topo show` - Show topology configuration
- `topo config <instance> <type> <accel> [func] [midi_cc]` - Configure topology instance
  - `instance`: Topology instance (0-5)
  - `type`: 1=T1 (1 accel), 2=T2 (2 accel), 3=T3 (1 accel), 4=T4 (2 accel)
  - `accel`: Axis index 0-5 (X,Y,Z,Roll,Pitch,Yaw), for T2/T4 use comma-separated like '0,1'
  - `func`: Function unit index 0-7 (optional)
  - `midi_cc`: MIDI CC number 0-127 (optional)
- `topo mixer <0-4>` - Set mixer type for combining inputs
  - 0=PASSTHROUGH, 1=SUM, 2=AVERAGE, 3=MAX, 4=MIN

#### Function Unit Commands (`func` submenu)
- `func show [idx]` - Show function unit configuration (all or specific index)
- `func linear <idx> <in_min> <in_max> <out_min> <out_max>` - Configure LINEAR function
  - Maps input range [in_min, in_max] to output range [out_min, out_max]

#### Virtual Port Debug Commands (`vport` submenu)
- `vport show <instance> [vport_offset]` - Show virtual port value for debugging
  - `instance`: Topology instance (0-5)
  - `vport_offset`: Virtual port offset within instance (0-2, optional)

#### Accelerometer Pipeline Commands (`pipeline` submenu)
- `pipeline set <rho> <theta> <midi_cc> <func_type> [params...]` - Configure rotation pipeline
  - `rho`: Rotation around X axis (0-360 degrees)
  - `theta`: Rotation around Y axis (0-360 degrees)
  - `midi_cc`: MIDI CC number (0-127)
  - `func_type`: Conversion function type
    - `linear <scale> <offset>` - Linear mapping (scale: -10.0 to 10.0, offset: -1.0 to 1.0)
    - `exponential <exponent>` - Exponential curve (exponent: 0.1-5.0, <1.0=log feel, >1.0=exp feel)
    - `scurve <steepness>` - S-curve response (steepness: 1.0-20.0)
    - `lookup <v0> <v1> <v2> <v3> <v4>` - Lookup table with 5 MIDI values (0-127)
  - Examples:
    - `pipeline set 45 90 1 linear 1.0 0.0` - Basic linear mapping
    - `pipeline set 45 90 1 linear -1.0 0.0` - Reversed output
    - `pipeline set 30 60 7 exponential 2.0` - Exponential response
    - `pipeline set 0 180 11 scurve 10.0` - S-curve response
    - `pipeline set 15 45 74 lookup 0 32 64 96 127` - Custom lookup table
- `pipeline show` - Display current pipeline configuration (human-readable)
- `pipeline json` - Display pipeline configuration in JSON format
  - Example output:
    ```json
    {
      "patch": 0,
      "rotation": {
        "rho_degrees": 45.0,
        "theta_degrees": 90.0
      },
      "output": {
        "midi_cc": 1
      },
      "conversion": {
        "function_type": "linear",
        "parameters": {
          "scale": 1.00,
          "offset": 0.00
        }
      }
    }
    ```

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

--- GLOBAL SETTINGS ---
Active patch: 0
MIDI:
  Channel: 1
BLE:
  Max guitars: 4
  Scan interval: 100 ms
LED:
  Brightness: 128
Accelerometer:
  Scale (mg): X=±1000, Y=±1000, Z=±1000 (full scale G-force → MIDI 0-127)
  Offset (mg): X=0, Y=0, Z=0 (center point → MIDI 64)
  Ranges: X=[-1000:1000], Y=[-1000:1000], Z=[-1000:1000] mg → MIDI[0:127]
Filters:
  Running average: Enabled
  Average depth: 5 samples

--- PATCH SETTINGS (Patch 0) ---
LED:
  Mode: 0
Accelerometer:
  Deadzone: 1

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
MIDI channel set to 2 (global setting)

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
MIDI channel set to 5 (global setting)
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
      "accel_offset": [0, 0, 0, 0, 0, 0],
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

**Field Descriptions:**

- **Global Configuration:**
  - `default_patch`: Currently active patch (0-15)
  - `midi_channel`: MIDI output channel (0-15, maps to MIDI channels 1-16)
  - `max_guitars`: Maximum number of BLE clients (1-4)
  - `ble_scan_interval_ms`: BLE scan interval in milliseconds (10-1000)
  - `led_brightness`: LED brightness level (0-255)
  - `accel_scale`: Full-scale G-force in milli-g for each axis [X,Y,Z,Roll,Pitch,Yaw] that maps to MIDI 0-127
  - `accel_offset`: Center point offset in milli-g for each axis that maps to MIDI 64
  - `running_average_enable`: Enable/disable running average filter (true/false)
  - `running_average_depth`: Running average sample depth (3-10)

- **Patch Configuration:**
  - `patch_num`: Patch number (0-15)
  - `patch_name`: User-defined patch name (up to 31 characters)
  - `velocity_curve`: Velocity curve type (0-127, legacy field)
  - `cc_mapping`: MIDI CC numbers for 6 axes [X,Y,Z,Roll,Pitch,Yaw] (0-127)
  - `led_mode`: LED mode (0-3)
  - `accel_deadzone`: CC change threshold to prevent jitter (0-127)
  - `accel_min`: Minimum CC output values for each axis (0-127, legacy field)
  - `accel_max`: Maximum CC output values for each axis (0-127, legacy field)
  - `accel_invert`: Bitfield for axis inversion (legacy field)

**Note:** The `rotation_pipeline` configuration (used by the accelerometer pipeline feature) is stored per-patch but is accessed via the separate `pipeline` commands, not through `config export`.

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

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
- `config midi_ch <1-16>` - Set MIDI output channel
- `config cc <x|y|z> <0-127>` - Set CC number for each accelerometer axis
- `config unlock_default` - Unlock DEFAULT config area (development only)
- `config write_default` - Write factory defaults (manufacturing only)
- `config erase_all` - Erase all configuration (testing only)

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

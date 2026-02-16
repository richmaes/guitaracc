# Basestation UI Interface

## Overview
The basestation provides a serial UART interface for user interaction, configuration, and debugging. This interface is separate from the MIDI output and operates on UART1.

## Hardware Configuration

### UART1 - UI/Config Interface
- **Port**: UART1 on Application Core
- **Baud Rate**: 115200
- **Physical Connection**: VCOM0 (USB Virtual COM Port)

#### Pin Assignment
| Function | Pin   | Direction |
|----------|-------|-----------|
| TX       | P1.05 | Output    |
| RX       | P1.04 | Input     |
| RTS      | P1.07 | Output    |
| CTS      | P1.06 | Input     |

#### Hardware Flow Control
- Full hardware flow control (RTS/CTS) is enabled
- Flow control pins are configured but may not be used by all terminal applications

## Current Features

### Command Interface
- **Status**: Active
- **Function**: Text-based command interface with line editing
- **Commands Available**:
  - `help` - Display available commands
  - `status` - Show system status (connected devices, MIDI state, config area)
  - `echo on|off` - Toggle character echo mode
  - `clear` - Clear terminal screen (VT100)
  - `config show` - Display current configuration
  - `config save` - Save configuration to flash
  - `config restore` - Restore factory defaults
  - `config midi_ch <1-16>` - Set MIDI channel
  - `config cc <x|y|z> <0-127>` - Set CC number for accelerometer axis
  - `config unlock_default` - Unlock factory default area (development only)
  - `config write_default` - Write factory defaults (requires unlock)

### Command Processing
- Line buffering (128 character buffer)
- Backspace/delete support
- Echo mode (on by default)
- Carriage return and newline handling
- Command prompt: `GuitarAcc> `

### Welcome Banner
On connection, displays:
```
========================================
  GuitarAcc Basestation v1.0
  Type 'help' for available commands
========================================
GuitarAcc> 
```

## Device Tree Configuration

Location: `basestation/app.overlay`

```dts
&uart1 {
    status = "okay";
    current-speed = <115200>;
    pinctrl-0 = <&uart1_default>;
    pinctrl-1 = <&uart1_sleep>;
    pinctrl-names = "default", "sleep";
};
```

## Code Implementation
### Module Structure
The UI interface is implemented as a separate module:
- `src/ui_interface.h` - Public API
- `src/ui_interface.c` - Command processing implementation

### Public API
```c
/* Initialize the UI interface */
int ui_interface_init(const struct device *uart_dev);

/* Process incoming character (called from ISR) */
void ui_interface_process_char(char c);

/* Print formatted message to UI */
void ui_print(const char *fmt, ...);

/* Update system status */
void ui_interface_update_status(int connected_count, bool midi_active);
```
### Initialization
```c
/* UI/Config UART device (UART1 on VCOM0) */
static const struct device *ui_uart;

/* In main() */
ui_uart = DEVICE_DT_GET(DT_NODELABEL(uart1));
if (!device_is_ready(ui_uart)) {
    LOG_WRN("UI UART device not ready - UI interface unavailable");
} else {
    /* Set up RX interrupt for command interface */
    uart_irq_callback_set(ui_uart, ui_uart_isr);
    uart_irq_rx_enable(ui_uart);
    
    /* Initialize UI interface module */
    err = ui_interface_init(ui_uart);
    if (err) {
        LOG_ERR("Failed to initialize UI interface (err %d)", err);
    }
}
```

### Interrupt Handler
```c
static void ui_uart_isr(const struct device *dev, void *user_data)
{
    uart_irq_update(dev);
    
    if (uart_irq_rx_ready(dev)) {
        uint8_t byte;
        while (uart_fifo_read(dev, &byte, 1) > 0) {
            /* Process character through UI interface */
            ui_interface_process_char((char)byte);
        }
    }
}
```

### Status Updates
The system automatically updates status on connection changes:
```c
/* On device connection */
ui_interface_update_status(1, true);

/* On device disconnection */
ui_interface_update_status(0, false);
```

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
========================================
  GuitarAcc Basestation v1.0
  Type 'help' for available commands
========================================
GuitarAcc> help

Available commands:
  help                     - Show this help message
  status                   - Show system status
  echo                     - Toggle echo mode (on/off)
  clear                    - Clear screen
  config show              - Show current configuration
  config save              - Save configuration to flash
  config restore           - Restore factory defaults
  config midi_ch <1-16>    - Set MIDI channel
  config cc <x|y|z> <0-127> - Set CC number for axis
  config unlock_default    - Unlock DEFAULT area (dev only!)
  config write_default     - Write factory default (requires unlock!)

GuitarAcc> status

=== GuitarAcc Basestation Status ===
Connected devices: 1
MIDI output: Active
Echo mode: On
Config area: A (seq=5)

GuitarAcc> config show

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

GuitarAcc> config midi_ch 2

MIDI channel set to 2

GuitarAcc> config cc x 74

X-axis CC set to 74

GuitarAcc> 
```
Configuration System

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
1. **Debug Mode**
   - Real-time accelerometer value display
   - MIDI message monitoring
   - BLE connection statistics

2. **Advanced Configuration**
   - Accelerometer deadzone adjustment
   - Scale/sensitivity per axis
   - LED brightness and modes
   - Velocity curve selection

3. **Status Output**
   - Connection status notifications
   - MIDI activity indicators
   - Enhanced error reportingI output: Active
```

## Pin Conflict Resolution

The UI interface pins (P1.04-P1.07) were carefully chosen to avoid conflicts:

| Pin Range | Usage |
|-----------|-------|
| P1.04-P1.07 | UART1 UI Interface |
| P1.08-P1.11 | UART0 MIDI Output |
| P1.12-P1.15 | Network Core UART (forwarded via GPIO) |

## Related Documentation
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture overview
- [MAPPING.md](MAPPING.md) - Accelerometer to MIDI mapping
- [REFACTORING.md](REFACTORING.md) - Code organization

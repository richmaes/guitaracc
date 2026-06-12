# GuitarAcc Command Line Interface

## Overview
The GuitarAcc basestation provides a comprehensive command-line interface via **Zephyr Shell** over USB serial connection. This interface enables configuration management, system monitoring, MIDI diagnostics, and accelerometer mapping control.

## Connection Details

**USB Serial Port (VCOM0):**
- **Baud Rate**: 115200
- **Settings**: 8N1, no flow control
- **Port Location**:
  - macOS/Linux: `/dev/tty.usbmodem*`
  - Windows: `COMx`

## Features

The CLI provides access to:
- **System Status**: Connected devices, MIDI output state, configuration area
- **Configuration Management**: Patch selection, parameter adjustment, import/export
- **MIDI Control**: Real-time message transmission, receive statistics, program changes
- **Accelerometer Pipeline**: Rotation-based axis mapping with customizable conversion functions
- **Real-time Monitoring**: Live pipeline data snapshots showing transformation stages

## Command Structure

Commands are organized hierarchically:
- `status` - System status and monitoring
- `monitor` - Real-time pipeline data snapshot
- `config` - Configuration and patch management
- `midi` - MIDI output and monitoring
- `pipeline` - Accelerometer rotation pipeline configuration
- Additional Zephyr shell built-ins (`help`, `clear`, `history`, etc.)

### Shell Features
- Tab completion for commands and parameters
- Command history with up/down arrow navigation
- Integrated logging output
- ANSI color support for readability
- Built-in help system (`help` or `<command> -h`)

## Key Commands

### Status & Monitoring
- `status` - Show system status (connections, MIDI state, config)
- `monitor [json]` - Display real-time pipeline snapshot showing all transformation stages

### Pipeline Configuration
- `pipeline set <rho> <theta> <cc> <func> [params...]` - Configure rotation-based accelerometer mapping
  - **rho**: Rotation around X-axis (0-360°)
  - **theta**: Rotation around Y-axis (0-360°)
  - **cc**: Output MIDI CC number (0-127)
  - **func**: Conversion function type (linear, exponential, scurve, lookup)
- `pipeline show` - Display current pipeline configuration
- `pipeline json` - Show pipeline configuration in JSON format

### Configuration Management
- `config show` - Display all configuration values
- `config save` - Save configuration to flash
- `config select <0-15>` - Select active patch
- `config export [global | patch <N>]` - Export configuration as JSON
- `config import` - Import configuration from JSON (interactive)

### MIDI Control
- `midi rx_stats` - Show MIDI receive statistics (clock, BPM, messages)
- `midi program [0-127]` - Get or set MIDI program number
- `midi send_rt <0xF8-0xFF>` - Send real-time MIDI message

## Documentation

**→ See [basestation/UI_INTERFACE.md](basestation/UI_INTERFACE.md) for complete command reference and implementation details**

**→ See [docs/specs/serial-protocol.md](docs/specs/serial-protocol.md) for protocol specification and JSON formats**

**→ See [basestation/ACCEL_ROTATION_PIPELINE.md](basestation/ACCEL_ROTATION_PIPELINE.md) for details on the rotation pipeline system**

## Accelerometer Mapping Approach

The system uses a **rotation-based pipeline** for mapping accelerometer data to MIDI:

1. **Rotation Stage**: Apply rho (X-axis) and theta (Y-axis) rotations to define a virtual axis
2. **Projection Stage**: Project 3D acceleration onto the rotated axis (scalar value -1.0 to 1.0)
3. **Conversion Stage**: Apply a function (linear, exponential, s-curve, or lookup table) to transform the projection
4. **MIDI Output**: Convert to MIDI CC value (0-127) and transmit

This approach provides more flexibility than simple per-axis CC mapping, allowing custom virtual axes at any orientation.

## Python Test Scripts

The project includes Python scripts for automated CLI testing:
- [test_config_commands.py](basestation/test_config_commands.py) - Configuration command validation
- [test_shell.py](basestation/test_shell.py) - General shell command testing
- [select_port.py](basestation/select_port.py) - Utility for port selection

See [basestation/TEST_SCRIPTS.md](basestation/TEST_SCRIPTS.md) for test script documentation.

## Usage Examples

### Connect to Shell
```bash
# macOS/Linux
screen /dev/tty.usbmodem* 115200

# Or use Python test scripts
python3 basestation/test_shell.py
```

### View System Status
```
uart:~$ status
```

### Configure Accelerometer Pipeline
Set up a rotation-based mapping with linear conversion:
```
uart:~$ config select 0
uart:~$ pipeline set 45 90 1 linear 1.0 0.0
uart:~$ config save
```

Configure exponential curve for more responsive control:
```
uart:~$ pipeline set 30 60 7 exponential 2.0
```

### Monitor Real-Time Pipeline Data
View the transformation pipeline in action:
```
uart:~$ monitor
```

Get JSON output for external monitoring:
```
uart:~$ monitor json
```

### View Pipeline Configuration
```
uart:~$ pipeline show
uart:~$ pipeline json
```

### Export Configuration
```
uart:~$ config export
```

### Monitor MIDI Input
```
uart:~$ midi rx_stats
```

## Related Documentation
- [basestation/UI_INTERFACE.md](basestation/UI_INTERFACE.md) - Full command reference and implementation
- [docs/specs/serial-protocol.md](docs/specs/serial-protocol.md) - Protocol specification for external tools
- [basestation/ACCEL_ROTATION_PIPELINE.md](basestation/ACCEL_ROTATION_PIPELINE.md) - Rotation pipeline system design
- [basestation/CONFIG_STORAGE.md](basestation/CONFIG_STORAGE.md) - Configuration persistence details
- [basestation/MAPPING.md](basestation/MAPPING.md) - Legacy mapping documentation
- [basestation/TEST_SCRIPTS.md](basestation/TEST_SCRIPTS.md) - Python test scripts for CLI validation

# Serial Protocol Specification

## Overview
This document defines the USB/Serial communication protocol for configuring and monitoring the GuitarAcc basestation.

## Communication Parameters
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

## Command Interface

### Shell Commands
The basestation exposes a Zephyr shell interface over USB/Serial CDC-ACM.

#### Configuration Commands
```
config get <key>           - Get configuration value
config set <key> <value>   - Set configuration value
config save                - Save configuration to flash
config load                - Load configuration from flash
config reset               - Reset to factory defaults
config export              - Export configuration as JSON
config import <json>       - Import configuration from JSON
```

#### Mapping Commands
```
mapping list               - List all accelerometer mappings
mapping add <params>       - Add new mapping
mapping remove <id>        - Remove mapping by ID
mapping enable <id>        - Enable mapping
mapping disable <id>       - Disable mapping
```

#### Status Commands
```
status                     - Show system status
version                    - Show firmware version
```

See [basestation/UI_INTERFACE.md](../../basestation/UI_INTERFACE.md) for complete command reference.

## JSON Configuration Format

### Configuration Export Format
```json
{
  "version": "1.0.0",
  "mappings": [
    {
      "id": 0,
      "enabled": true,
      "source": "accel_x",
      "cc_number": 1,
      "min_value": 0,
      "max_value": 127,
      "curve": "linear"
    }
  ],
  "settings": {
    "midi_channel": 0,
    "update_rate": 100
  }
}
```

## Response Format

### Success Response
```
OK: <message>
```

### Error Response
```
ERROR: <error message>
```

### JSON Output
When using `--json` flag:
```json
{
  "status": "success|error",
  "message": "...",
  "data": { }
}
```

## See Also
- [basestation/CONFIG_STORAGE.md](../../basestation/CONFIG_STORAGE.md) - Configuration persistence
- [basestation/test_config_commands.py](../../basestation/test_config_commands.py) - Test scripts

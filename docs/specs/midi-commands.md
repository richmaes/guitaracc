# MIDI Commands Specification

## Overview
This document defines the MIDI message formats and command structure used in the GuitarAcc system.

## MIDI Message Types

### Control Change (CC) Messages
Used for continuous controller data from accelerometer mappings.

**Format**: `0xBn cc vv`
- `n`: MIDI channel (0-15)
- `cc`: Controller number (0-127)
- `vv`: Controller value (0-127)

### Program Change Messages
Used for patch/preset selection.

**Format**: `0xCn pp`
- `n`: MIDI channel (0-15)
- `pp`: Program number (0-127)

### System Real-Time Messages
- **Clock**: `0xF8` - MIDI timing clock
- **Start**: `0xFA` - Start playback
- **Continue**: `0xFB` - Continue playback
- **Stop**: `0xFC` - Stop playback

## Accelerometer to MIDI Mapping

### Mapping Configuration
Each accelerometer axis can be mapped to a MIDI control change:
- **Source**: X, Y, or Z axis
- **Destination**: CC number (0-127)
- **Range**: Min/Max values for scaling
- **Curve**: Linear, exponential, or custom

See [basestation/MAPPING.md](../../basestation/MAPPING.md) for implementation details.

## Output Routing
MIDI messages can be sent via:
1. **BLE MIDI**: Over Bluetooth to connected devices
2. **USB MIDI**: Over USB connection to host
3. **Serial**: For debugging and monitoring

## See Also
- [ble-protocol.md](ble-protocol.md) - BLE transport layer
- [serial-protocol.md](serial-protocol.md) - USB/Serial commands

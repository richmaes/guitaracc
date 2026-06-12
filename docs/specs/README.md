# GuitarAcc Protocol Specifications

This directory contains the protocol specifications for the GuitarAcc system. These specifications are the source of truth for communication between the firmware (basestation/client) and external control applications (GUI, CLI tools, etc.).

## Specifications

### [ble-protocol.md](ble-protocol.md)
Bluetooth Low Energy protocol for basestation ↔ client communication.

### [midi-commands.md](midi-commands.md)
MIDI message formats and accelerometer mapping specifications.

### [serial-protocol.md](serial-protocol.md)
USB/Serial protocol for configuration and monitoring via shell commands.

## For GUI Developers

When building applications that interface with the GuitarAcc basestation:

1. **Configuration Interface**: Use the serial protocol defined in [serial-protocol.md](serial-protocol.md)
2. **MIDI Output**: Monitor MIDI messages as defined in [midi-commands.md](midi-commands.md)
3. **BLE Communication**: Reference [ble-protocol.md](ble-protocol.md) for wireless communication

## Version Tracking

These specifications should be updated whenever:
- Protocol changes are made to the firmware
- New commands are added
- Message formats change
- Configuration structure changes

All changes should be committed with the corresponding firmware implementation.

## Related Documentation

- [ARCHITECTURE.md](../../ARCHITECTURE.md) - System architecture overview
- [basestation/UI_INTERFACE.md](../../basestation/UI_INTERFACE.md) - UI/CLI interface details
- [basestation/MAPPING.md](../../basestation/MAPPING.md) - Accelerometer mapping implementation
- [basestation/CONFIG_STORAGE.md](../../basestation/CONFIG_STORAGE.md) - Configuration persistence

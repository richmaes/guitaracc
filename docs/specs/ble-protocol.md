# BLE Protocol Specification

## Overview
This document defines the Bluetooth Low Energy protocol used for communication between the GuitarAcc basestation and client devices.

## BLE Roles
- **Basestation**: nRF5340 Audio DK - Central/Client role
- **Client**: Thingy:53 - Peripheral/Server role

## Services and Characteristics

### Accelerometer Data Service
**Service UUID**: TBD

#### Accelerometer Data Characteristic
**Characteristic UUID**: TBD
- **Properties**: Notify
- **Data Format**: TBD
- **Update Rate**: TBD

### MIDI Output Service
**Service UUID**: TBD

#### MIDI Messages Characteristic
**Characteristic UUID**: TBD
- **Properties**: Write, Notify
- **Data Format**: See [midi-commands.md](midi-commands.md)

## Connection Parameters
- **Connection Interval**: TBD
- **Latency**: TBD
- **Supervision Timeout**: TBD

## MTU Requirements
- **Minimum MTU**: TBD
- **Recommended MTU**: TBD

## See Also
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - System architecture overview
- [midi-commands.md](midi-commands.md) - MIDI message formats

# GuitarAcc Architecture

## System Overview
A BLE-based guitar accessory system that connects multiple guitars to a central basestation, which outputs MIDI data via serial port.

## BLE Roles

### Basestation (CENTRAL)
- **Hardware**: nRF5340 Audio DK
- **Board Target**: `nrf5340_audio_dk_nrf5340_cpuapp`
- **Role**: BLE Central (Master)
- **Responsibilities**:
  - Actively scans for guitar peripherals
  - Connects to up to 4 guitars simultaneously
  - Receives data from connected guitars
  - Outputs MIDI stream via serial port (31.25 kHz baud)

### Guitar Clients (PERIPHERAL)
- **Hardware**: Thingy:53
- **Board Target**: `thingy53_nrf5340_cpuapp`
- **Role**: BLE Peripheral (Slave)
- **Responsibilities**:
  - Advertises presence as "GuitarAcc Guitar"
  - Waits for basestation to initiate connection
  - Sends guitar data to basestation when connected

## Data Flow
```
Guitar 1 (Peripheral) ──┐
Guitar 2 (Peripheral) ──┤
Guitar 3 (Peripheral) ──┼──> Basestation (Central) ──> MIDI Serial Output
Guitar 4 (Peripheral) ──┘
```

## BLE UUIDs

### Guitar Service UUID
- **UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`
- **Purpose**: Identifies GuitarAcc guitar devices
- **Usage**: 
  - Advertised by all guitar peripherals
  - Used by basestation to filter and connect only to guitar devices
  - Prevents accidental connections to non-guitar BLE devices

## Key Concepts

### Why Central vs Peripheral?
- **Peripheral (Slave)**: Advertises and waits for connections. Lower power, simpler. Perfect for battery-powered guitars.
- **Central (Master)**: Scans and initiates connections. Can connect to multiple devices. Required for the basestation to manage multiple guitars.

### MIDI Output
- Standard MIDI baud rate: 31,250 bps
- Output via UART on basestation
- Aggregates data from all connected guitars into single MIDI stream

## Connection Management
- Basestation maintains array of up to 4 guitar connections
- Automatically resumes scanning when connection slots available
- Handles reconnection when guitars disconnect

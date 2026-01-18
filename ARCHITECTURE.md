# GuitarAcc System Architecture

## System Overview
A BLE-based guitar accessory system that connects multiple guitar-mounted devices to a central basestation. The system captures motion data from guitars and outputs MIDI commands via serial connection.

```
Client 1 (Thingy:53) ──┐
Client 2 (Thingy:53) ──┤
Client 3 (Thingy:53) ──┼──> Basestation (nRF5340 Audio DK) ──> MIDI Serial Output
Client 4 (Thingy:53) ──┘
          BLE                                                    31.25 kbaud UART
```

## System Components

### Basestation Application
- **Hardware**: nRF5340 Audio DK
- **Board Target**: `nrf5340_audio_dk_nrf5340_cpuapp`
- **Role**: BLE Central (scans and connects)
- **Responsibilities**:
  - Scans for and connects to up to 4 guitar clients
  - Receives acceleration data from connected clients
  - Converts motion data to MIDI commands
  - Outputs MIDI stream via UART at 31.25 kbaud

**→ See [basestation/ARCHITECTURE.md](basestation/ARCHITECTURE.md) for detailed implementation**

### Client Application
- **Hardware**: Thingy:53
- **Board Target**: `thingy53_nrf5340_cpuapp`
- **Role**: BLE Peripheral (advertises and waits)
- **Responsibilities**:
  - Reads accelerometer data (ADXL362) at 10Hz
  - Advertises as "GuitarAcc Guitar"
  - Transmits acceleration data over BLE notifications
  - Manages power states (active/sleep) based on motion

**→ See [client/ARCHITECTURE.md](client/ARCHITECTURE.md) for detailed implementation**

## BLE Communication

### Guitar Service
- **UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`
- **Purpose**: Identifies GuitarAcc devices
- **Advertised by**: All client devices
- **Used by**: Basestation for device discovery and filtering

### Acceleration Characteristic
- **UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a40`
- **Properties**: Notify only
- **Data Format**: 6 bytes [X: int16][Y: int16][Z: int16] in milli-g
- **Update Rate**: 10Hz when connected (change-based to save power)

## Data Flow

1. **Client** reads 3-axis accelerometer data
2. **Client** converts to milli-g and packs into 6-byte structure
3. **Client** sends data via BLE GATT notification (only when changed)
4. **Basestation** receives data from all connected clients
5. **Basestation** processes motion data through MIDI logic
6. **Basestation** outputs MIDI commands via UART

## Power Management

### Client Power States
- **Active Mode**: Full sampling at 10Hz, advertising or connected
- **Sleep Mode**: Reduced 2Hz polling, advertising stopped after 30s of no motion
- **Wake-on-Motion**: Automatically resumes when motion detected

### Connection Behavior
- **Central Role**: Basestation continuously scans for available clients
- **Peripheral Role**: Clients advertise when active, stop when sleeping
- **Max Connections**: Basestation supports up to 4 simultaneous client connections

## Development Environment

- **SDK**: Nordic nRF Connect SDK with Zephyr RTOS
- **IDE**: VS Code with nRF Connect extension
- **Build System**: CMake with Zephyr build system
- **Testing**: Host-based unit tests for logic modules (see basestation/REFACTORING.md)

## Building the Applications

### Basestation
```bash
cd basestation
make build
```

### Client
```bash
cd client
make build
```

**Note**: See individual README.md files in each application directory for detailed build and flash instructions.

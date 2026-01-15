# GuitarAcc System Architecture

## Hardware Platform

**Development Board**: nRF5340 Audio DK  
**Target Board**: Nordic Thingy:53 (for client/guitar device)  
**MCU**: nRF5340 (Dual-core ARM Cortex-M33)

## System Overview

The GuitarAcc system consists of two main applications:

1. **Client Application** - Runs on Thingy:53, reads guitar accelerometer data
2. **Basestation Application** - Runs on nRF5340 Audio DK, receives BLE data and outputs MIDI

## Pin Assignments - nRF5340 Audio DK

### Application Core (CPUAPP) Pinout

#### UART0 (MIDI Output - **SWAPPED PINS**)
- **TX**: P1.9
- **RX**: P1.8
- **RTS**: P1.10
- **CTS**: P1.11

*Note: These pins were originally assigned to Network Core UART0 in the board default configuration. They have been swapped via device tree overlay.*

#### UART1
- **Status**: DISABLED (to avoid pin conflict with UART0)

#### I2C1 (TWIM)
- **SDA**: P1.2
- **SCL**: P1.3

#### I2S0 (Audio)
- **MCK**: P0.12
- **SCK_M**: P0.14
- **LRCK_M**: P0.16
- **SDOUT**: P0.13
- **SDIN**: P0.15

#### SPI4 (SPIM)
- **SCK**: P0.8
- **MOSI**: P0.9
- **MISO**: P0.10

#### PWM0
- **OUT0**: P0.7
- **OUT1**: P0.25
- **OUT2**: P0.26

### Network Core (CPUNET) Pinout

#### UART0 (**SWAPPED PINS**)
- **TX**: P1.5
- **RX**: P1.4
- **RTS**: P1.7
- **CTS**: P1.6

*Note: These pins were originally assigned to Application Core UART0 in the board default configuration. They have been swapped via device tree overlay (hci_ipc.overlay).*

**Note**: The network core UART0 now uses pins that were originally the application core's UART0. In the basestation application, the network core is used for Bluetooth HCI IPC and does not use UART.

#### SPI0 (SPIM)
- **SCK**: P0.8
- **MOSI**: P0.9
- **MISO**: P0.10

**Note**: The network core SPI0 shares pins with the application core's SPI4.

## Important Pin Conflicts

**⚠️ UART Pin Configuration - SWAPPED**:
- Application Core UART0 now uses: P1.9 (TX), P1.8 (RX), P1.10 (RTS), P1.11 (CTS)
- Network Core UART0 now uses: P1.5 (TX), P1.4 (RX), P1.7 (RTS), P1.6 (CTS)
- **These pins have been swapped from the board defaults via device tree overlays**
- Application Core UART1 is **disabled** to avoid conflict with UART0

**⚠️ SPI Pin Overlap**:
- Application Core SPI4 and Network Core SPI0 share P0.8, P0.9, P0.10
- **These peripherals cannot be used simultaneously**

## Basestation Application Configuration

### UART Configuration
- **Port**: UART0 on Application Core
- **Pins**: TX on P1.9, RX on P1.8 (swapped from default)
- **Baud Rates**:
  - Test Mode: 115200 (for VCOM monitoring)
  - Production Mode: 31250 (MIDI standard)
- **Control**: `USE_VCOM_BAUD` define in main.c
- **Device Tree**: Pin configuration overridden in app.overlay

### Bluetooth Configuration
- **Role**: Central (scans and connects to guitars)
- **Max Connections**: 4 guitars simultaneously
- **Service UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`
- **Device Name Filter**: "GuitarAcc Guitar"

### Test Mode
- **Enable/Disable**: `TEST_MODE_ENABLED` define in main.c
- **Test Interval**: 1000ms (1 second)
- **Test Message**: MIDI Note ON/OFF (Middle C, Channel 0)
  - Note ON: `0x90 0x3C 0x40`
  - Note OFF: `0x80 0x3C 0x00`

## Client Application Configuration

### Target Hardware
- **Board**: Nordic Thingy:53 (thingy53/nrf5340/cpuapp)
- **Accelerometer**: ADXL362

### Bluetooth Configuration
- **Role**: Peripheral
- **Device Name**: "GuitarAcc Guitar"
- **Service UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`

### Power Management
- Motion-based wake/sleep
- Low-power accelerometer configuration

## Build System

Both applications use:
- **Build System**: CMake + Ninja
- **SDK**: Nordic nRF Connect SDK v3.1.1 / Zephyr 3.7.99
- **Toolchain**: nrfutil toolchain-manager
- **Multi-core**: Automatic building of network core (HCI IPC) and bootloader (MCUboot)

### Build Outputs
- `merged_domains.hex` - Combined application + network core firmware
- `merged_CPUNET.hex` - Network core only
- `app_signed.hex` - Signed application for DFU
- `mcuboot.hex` - Bootloader

## Communication Protocol

### BLE → MIDI Mapping
TBD: Define how accelerometer data is translated to MIDI messages

### MIDI Output Format
- **Protocol**: MIDI 1.0
- **Connection**: 5-pin DIN or USB-MIDI
- **Baud**: 31250 (MIDI standard)
- **Message Types**: Note ON, Note OFF, Control Change (TBD)

## Development Workflow

1. **Configure**: `cmake -B build -GNinja -DBOARD=<board>`
2. **Build**: `cmake --build build`
3. **Flash**: Use nRF Connect extension "Flash" button
4. **Monitor**: VCOM0 at 115200 baud (test mode)

## Future Enhancements

- [ ] Define accelerometer → MIDI mapping algorithm
- [ ] Add MIDI channel assignment per guitar
- [ ] Implement pitch bend and mod wheel from motion
- [ ] Add wireless DFU support
- [ ] Battery monitoring for Thingy:53
- [ ] LED status indicators

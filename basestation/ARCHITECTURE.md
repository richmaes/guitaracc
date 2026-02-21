# GuitarAcc System Architecture

## Documentation

- **[REFACTORING.md](REFACTORING.md)** - Code organization and host-based testing framework
- **[MAPPING.md](MAPPING.md)** - Accelerometer to MIDI value mapping abstraction layer
- **[UI_INTERFACE.md](UI_INTERFACE.md)** - Serial UI interface and configuration system

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
- **TX**: P1.8 (swapped)
- **RX**: P1.9 (swapped)
- **RTS**: P1.10
- **CTS**: P1.11

*Note: TX and RX have been swapped from the original pin assignments. These pins were originally assigned to Network Core UART0 in the board default configuration. Configuration modified via device tree overlay.*

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
- Application Core UART0 now uses: P1.8 (TX), P1.9 (RX), P1.10 (RTS), P1.11 (CTS)
- Network Core UART0 now uses: P1.5 (TX), P1.4 (RX), P1.7 (RTS), P1.6 (CTS)
- **TX and RX have been swapped from the original assignments via device tree overlays**
- Application Core UART1 is **disabled** to avoid conflict with UART0

**⚠️ SPI Pin Overlap**:
- Application Core SPI4 and Network Core SPI0 share P0.8, P0.9, P0.10
- **These peripherals cannot be used simultaneously**

## Basestation Application Configuration

### UART Configuration
- **Port**: UART0 on Application Core
- **Pins**: TX on P1.8, RX on P1.9 (TX and RX swapped)
- **Baud Rates**:
  - Test Mode: 115200 (for VCOM monitoring)
  - Production Mode: 31250 (MIDI standard)
- **Control**: `USE_VCOM_BAUD` define in main.c
- **Device Tree**: Pin configuration overridden in app.overlay

#### Interrupt-Driven UART Implementation
The UART uses **interrupt-driven transmission** for non-blocking MIDI output with priority queue support:

- **Dual Queue Architecture**:
  - **Priority Queue**: 8-byte buffer for real-time MIDI messages (0xF8-0xFF)
  - **Regular Queue**: 16-byte buffer for standard MIDI messages (Control Change, etc.)
- **ISR Handler**: `uart_isr()` automatically sends queued bytes when FIFO is ready
  - Always services priority queue first
  - Falls back to regular queue when priority queue is empty
- **Non-blocking**: `queue_midi_bytes()` and `queue_midi_rt_bytes()` return immediately
- **MIDI Receive**: RX interrupts enabled for bidirectional MIDI communication
- **MIDI Thru**: Real-time messages (0xF8-0xFF) automatically forwarded from input to output

**Important Notes:**
- nRF UART driver does **not** support `uart_configure()` (returns -ENOSYS)
- Baud rate **must** be configured in device tree (`current-speed` property in app.overlay)
- Change `current-speed = <115200>` to `<31250>` for production MIDI output
- ISR fills hardware FIFO (typically 16+ bytes) automatically
- Main code continues execution while UART transmits in background

**Queue Operation:**
1. Application calls `queue_midi_bytes(data, len)` for regular messages
2. Or calls `queue_midi_rt_bytes(data, len)` / `send_midi_realtime(byte)` for priority
3. Bytes added to appropriate circular buffer
4. TX interrupt enabled via `uart_irq_tx_enable()`
5. ISR fires when FIFO ready:
   - Checks priority queue first
   - Sends from regular queue if priority is empty
6. When both queues empty, ISR disables TX interrupt

**MIDI Receive Features:**
- RX interrupts process incoming MIDI data in real-time
- Statistics tracking for all real-time messages (Clock, Start, Stop, etc.)
- Program Change message detection and state tracking
- Automatic forwarding of real-time messages to output (MIDI thru)
- BPM calculation from MIDI Clock intervals

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
- **Accelerometer**: ADXL362 (SPI interface)
- **Connectivity**: Bluetooth LE (Peripheral role)
- **Power**: Battery-powered with sleep modes

### Accelerometer Configuration
- **Range**: ±2g (CONFIG_ADXL362_ACCEL_RANGE_2G)
- **Sampling Rate**: 10Hz when active, 2Hz when sleeping
- **Motion Threshold**: 0.5 m/s² deviation from 1g (gravity)
- **Interrupt Mode**: 0 (polling-based)
- **Power Management**: Automatic suspend/resume based on motion

### Bluetooth Configuration
- **Role**: Peripheral
- **Device Name**: "GuitarAcc Guitar"
- **Service UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`
- **Advertising**: Starts automatically on boot or wake from motion
- **Max Connections**: 1 (to basestation)

#### GATT Service Structure
**Guitar Service** (`a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`)
- **Acceleration Characteristic** (`a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a40`)
  - Properties: NOTIFY
  - Data Format: 6 bytes packed structure
    - X-axis: int16_t (milli-g)
    - Y-axis: int16_t (milli-g)  
    - Z-axis: int16_t (milli-g)
  - Update Rate: 10Hz when connected
  - Client Characteristic Configuration Descriptor (CCCD) for enabling/disabling notifications

### Power Management
- **Active Mode**: 
  - Accelerometer sampling at 10Hz
  - Bluetooth advertising/connected
  - Motion timeout: 30 seconds of inactivity
  
- **Sleep Mode**:
  - Triggered after 30 seconds without motion
  - Bluetooth advertising stopped
  - Accelerometer in low-power motion detection mode
  - Sampling reduced to 2Hz
  
- **Wake Conditions**:
  - Motion detected above threshold
  - Automatically resumes advertising
  - Resets motion timeout

### Motion Detection Algorithm
```c
// Calculate acceleration magnitude
magnitude = sqrt(x² + y² + z²)

// Detect motion (deviation from 1g gravity)
if (|magnitude - 9.81| > 0.5 m/s²) {
    motion_detected = true
}
```

### Data Flow (Current Implementation)
1. **Initialization**: 
   - Enable accelerometer
   - Enable Bluetooth
   - Start advertising
   - Start motion timer

2. **Active Mode**:
   - Sample accelerometer at 10Hz
   - Monitor for motion to reset sleep timer
   - Wait for basestation connection

3. **Connected**:
   - Stop motion timer (stay active)
   - Continue accelerometer monitoring at 10Hz
   - Wait for basestation to enable notifications (CCCD write)
   - Send acceleration data (X, Y, Z) via GATT notifications at 10Hz

4. **Sleep Mode**:
   - Reduce sampling to 2Hz
   - Stop advertising
   - Wake on motion detection

### Pin Assignments - Thingy:53

The Thingy:53 has most peripherals pre-configured on the board. Key interfaces:

#### ADXL362 Accelerometer (SPI)
- Connected via internal SPI bus
- Configured via device tree (accel0 alias)

#### Bluetooth
- Uses network core (nRF5340 CPUNET)
- HCI IPC communication with application core

#### Power
- Battery-powered
- Power management enabled
- USB charging support

### Future Enhancements (Client)
- [x] **GATT Service**: Implement custom service for acceleration data ✓
- [x] **GATT Characteristic**: Real-time acceleration streaming ✓
- [ ] **Interrupt-based motion**: Use ADXL362 hardware interrupts
- [ ] **Battery level reporting**: Add battery service
- [ ] **LED indicators**: Status feedback (connected/advertising/sleep)
- [ ] **Button support**: Manual wake or mode selection
- [ ] **Calibration**: Automatic or manual calibration routine
- [ ] **Data filtering**: Apply low-pass filter to acceleration data

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
The system uses a configurable abstraction layer to translate accelerometer data to MIDI values. See **[MAPPING.md](MAPPING.md)** for detailed information on:
- Linear mapping configuration
- Custom range setup
- API usage examples
- Testing framework

### MIDI Output Format
- **Protocol**: MIDI 1.0
- **Connection**: 5-pin DIN or USB-MIDI
- **Baud**: 31250 (MIDI standard)
- **Message Types**: 
  - Control Change (CC) for accelerometer data
  - Real-time messages (Clock, Start, Stop, etc.)
- **MIDI Thru**: Real-time messages received on MIDI IN are forwarded to MIDI OUT

### MIDI Program Change State
- **Default Program**: 1 (set on power-up)
- **Program Range**: 0-127 (MIDI standard)
- **State Management**: 
  - Automatically updated when PC messages received on MIDI IN
  - Can be queried or set via shell command: `midi program [0-127]`
  - Logged to console when changed
- **Future Use**: Program number will control mapping profiles, effects, or routing

## Development Workflow

1. **Configure**: `cmake -B build -GNinja -DBOARD=<board>`
2. **Build**: `cmake --build build`
3. **Flash**: Use nRF Connect extension "Flash" button
4. **Monitor**: VCOM0 at 115200 baud (test mode)

## User Interface (UI)

### Shell-Based Command Interface

The basestation provides a comprehensive command-line interface via **Zephyr Shell** over the USB VCOM port (115200 baud). The shell provides access to configuration, status monitoring, and MIDI diagnostics.

#### Available Commands

**Status Commands:**
- `status` - Display system status (connected devices, MIDI output state, config area)

**Configuration Commands:**
- `config show` - Display all current configuration values
- `config save` - Save current configuration to flash
- `config restore` - Restore factory default configuration
- `config midi_ch <1-16>` - Set MIDI output channel
- `config cc <x|y|z> <0-127>` - Set CC number for each axis
- `config unlock_default` - Unlock DEFAULT config area (development only)
- `config write_default` - Write factory defaults (manufacturing only)
- `config erase_all` - Erase all configuration (testing only)

**MIDI Commands:**
- `midi rx_stats` - Show MIDI receive statistics (bytes, clock messages, BPM, etc.)
- `midi rx_reset` - Reset MIDI receive statistics counters
- `midi program [0-127]` - Get or set current MIDI program number
- `midi send_rt <0xF8-0xFF>` - Send real-time MIDI message (Clock, Start, Stop, etc.)

#### Shell Features
- **Tab Completion**: Press Tab to autocomplete commands and show options
- **Command History**: Use Up/Down arrows to recall previous commands
- **Real-time Logging**: System logs displayed alongside command output
- **Color Support**: Commands use ANSI colors for better readability

#### Connection
- **Port**: USB VCOM (typically `/dev/tty.usbmodem*` on macOS)
- **Baud Rate**: 115200
- **Settings**: 8N1, no flow control
- **Terminal**: Any serial terminal (screen, minicom, pyserial, etc.)

#### Usage Example
```bash
# Connect via serial terminal
screen /dev/tty.usbmodem0010501494421 115200

# Check system status
uart:~$ status
Config area: A (seq=1)
=== GuitarAcc Basestation Status ===
Connected devices: 1
MIDI output: Active

# View MIDI receive statistics
uart:~$ midi rx_stats
=== MIDI RX Statistics ===
Total bytes received: 6162
Clock messages (0xF8): 6162
Clock interval: 28000 us (~89 BPM)
Start messages (0xFA): 0
Continue messages (0xFB): 0
Stop messages (0xFC): 0
Other messages: 0

# Check current program
uart:~$ midi program
Current MIDI Program: 1

# Change MIDI channel
uart:~$ config midi_ch 2
MIDI channel set to 2
```

### RGB LED Status Indicators

The basestation uses the onboard RGB LED (LED1) on the nRF5340 Audio DK to provide visual feedback for system state and connection status.

#### LED Color Mapping

| State | Color | Pattern | Meaning |
|-------|-------|---------|---------|
| **Initialization** | Yellow | Solid | System starting up |
| **Scanning** | Blue | Slow Blink (1Hz) | BLE scanning for clients |
| **1 Client Connected** | Green | Solid | One guitar connected |
| **2 Clients Connected** | Cyan | Solid | Two guitars connected |
| **3 Clients Connected** | Cyan | Slow Blink (1Hz) | Three guitars connected |
| **4 Clients Connected** | White | Solid | Four guitars connected (max) |
| **MIDI Active** | White | Brief Flash | MIDI data being transmitted |
| **Error** | Red | Fast Blink (4Hz) | System error state |

#### Implementation

- **Module**: `ui_led.c` / `ui_led.h`
- **Hardware**: RGB LED1 (led0, led1, led2 aliases in device tree)
- **GPIO Control**: Three separate GPIO pins for Red, Green, Blue channels
- **Update Rate**: LED patterns run in dedicated thread at appropriate intervals
- **Thread Priority**: 7 (low priority, non-critical)

#### API Functions

```c
int ui_led_init(void);                              // Initialize LED subsystem
void ui_led_set_state(ui_state_t state);            // Set system state
void ui_led_update_connection_count(uint8_t count); // Update based on connections
void ui_led_flash(ui_led_color_t color, uint32_t duration_ms); // Brief flash
```

#### Usage Example

```c
// During initialization
ui_led_init();
ui_led_set_state(UI_STATE_INIT);

// When scanning starts
ui_led_set_state(UI_STATE_SCANNING);

// When clients connect
ui_led_update_connection_count(num_connected);

// When sending MIDI
ui_led_flash(UI_LED_WHITE, 50); // 50ms flash
```

## Future Enhancements

- [ ] Define accelerometer → MIDI mapping algorithm
- [ ] Add MIDI channel assignment per guitar
- [ ] Implement pitch bend and mod wheel from motion
- [ ] Add wireless DFU support
- [ ] Battery monitoring for Thingy:53
- [x] LED status indicators

# GuitarAcc Client Application Architecture

## Documentation

- **[ACCELEROMETER.md](ACCELEROMETER.md)** - ADXL362 accelerometer configuration, API usage, and wake-on-motion details

## Hardware Platform

**Development Board**: Nordic Thingy:53  
**MCU**: nRF5340 (Dual-core ARM Cortex-M33)  
**Role**: BLE Peripheral (Guitar Device)  
**Accelerometer**: ADXL362 3-axis MEMS (see [ACCELEROMETER.md](ACCELEROMETER.md))

## System Overview

The client application runs on the guitar-mounted Thingy:53 device. It continuously monitors the guitar's motion using the onboard accelerometer and transmits acceleration data over Bluetooth Low Energy to the basestation when connected.

## Theory of Operation

### 1. **Initialization Phase**

On startup, the application performs the following initialization sequence:

1. **Accelerometer Setup** - Initializes the ADXL362 3-axis accelerometer (see [ACCELEROMETER.md](ACCELEROMETER.md))
2. **Bluetooth Stack Initialization** - Enables the BLE stack and registers GATT services
3. **Motion Timer Setup** - Configures a 30-second inactivity timer for power management
4. **Advertising Start** - Begins BLE advertising as a connectable peripheral

### 2. **Operating Modes**

#### Active Mode
- **Trigger**: Motion detected or connected to basestation
- **Behavior**:
  - Samples accelerometer at 10Hz (100ms intervals)
  - Transmits data over BLE when connected and notifications enabled
  - Monitors for motion to reset sleep timer
  - Full power consumption

#### Sleep Mode
- **Trigger**: 30 seconds of no motion detected while disconnected
- **Behavior**:
  - Stops BLE advertising to conserve power
  - Puts accelerometer in low-power suspend mode
  - Polls accelerometer at reduced rate (2Hz / 500ms intervals)
  - Automatically wakes on motion detection

#### Connected Mode
- **Trigger**: Basestation establishes BLE connection
- **Behavior**:
  - Disables sleep timer (stays active while connected)
  - Sends acceleration data only when it changes (power optimization)
  - Continues sampling at 10Hz
  - Returns to active mode on disconnect

### 3. **Data Flow**

```
Accelerometer (ADXL362)
         ↓
   Read XYZ axes
         ↓
   Convert to milli-g
   (1g = 9.81 m/s²)
         ↓
   Pack into 6-byte struct
   [X: 2 bytes][Y: 2 bytes][Z: 2 bytes]
         ↓
   Change Detection
   (only send if different)
         ↓
   BLE GATT Notification
   (if connected & enabled)
         ↓
   To Basestation
```

## Key Functions

### Main Application Functions

#### `int main(void)`
**Purpose**: Application entry point and main control loop  
**Theory**: Initializes all subsystems (LEDs, Bluetooth, accelerometer), starts BLE advertising, and enters infinite loop to monitor accelerometer and manage power states. LED initialization precedes Bluetooth setup to ensure visual feedback is available early in boot sequence  
**Returns**: 0 (never reached due to infinite loop)

#### `static void motion_timeout_handler(struct k_timer *timer)`
**Purpose**: Callback for motion inactivity timer  
**Theory**: After 30 seconds of no motion while disconnected, puts device into sleep mode to conserve battery. Stops advertising and suspends accelerometer  
**Parameters**:
- `timer`: Kernel timer that triggered the callback

#### `static void wake_from_motion(void)`
**Purpose**: Wakes device from sleep mode  
**Theory**: Called when motion is detected in sleep mode. Resumes accelerometer to full power, restarts BLE advertising, and resets the inactivity timer  
**Returns**: void

### BLE Connection Management

#### `static void connected(struct bt_conn *conn, uint8_t err)`
**Purpose**: Bluetooth connection established callback  
**Theory**: Called by Zephyr BLE stack when basestation connects. Stops motion timer since device should stay active while connected. Also turns on CON_STATUS_LED (LED2) to provide visual feedback of connection state  
**Parameters**:
- `conn`: BLE connection handle
- `err`: Error code (0 = success)

#### `static void disconnected(struct bt_conn *conn, uint8_t reason)`
**Purpose**: Bluetooth disconnection callback  
**Theory**: Called when connection to basestation is lost. Re-enables motion monitoring and resets notification state. Turns off CON_STATUS_LED (LED2) to indicate disconnection  
**Parameters**:
- `conn`: BLE connection handle
- `reason`: Disconnection reason code

### GATT Service Functions

#### `static void accel_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)`
**Purpose**: Client Characteristic Configuration (CCC) change callback  
**Theory**: Called when basestation enables/disables notifications on the acceleration characteristic. Sets global flag to control whether to send data  
**Parameters**:
- `attr`: GATT attribute that changed
- `value`: New CCC value (`BT_GATT_CCC_NOTIFY` = notifications enabled)

#### `static int send_accel_notification(struct bt_conn *conn)`
**Purpose**: Transmits current acceleration data over BLE  
**Theory**: Sends 6-byte notification containing X, Y, Z acceleration in milli-g. Only sends if notifications are enabled and data has changed from previous transmission  
**Parameters**:
- `conn`: BLE connection to send notification on
**Returns**: 0 on success, negative error code on failure

### Data Processing Functions

#### `static bool accel_data_changed(void)`
**Purpose**: Detects if acceleration data has changed  
**Theory**: Compares current acceleration values to previous transmission. Used to avoid sending redundant data and save power  
**Returns**: true if any axis changed, false if all unchanged

**Note**: For detailed information about accelerometer API functions, data formats, and configuration, see **[ACCELEROMETER.md](ACCELEROMETER.md)**.

## Data Structures

### `struct accel_data`
```c
struct accel_data {
    int16_t x;  /* X-axis in milli-g */
    int16_t y;  /* Y-axis in milli-g */
    int16_t z;  /* Z-axis in milli-g */
} __packed;
```
**Purpose**: Wire format for acceleration data transmission  
**Theory**: Uses signed 16-bit integers for ±32g range. Packed attribute ensures no padding, resulting in exactly 6 bytes on-wire

## BLE GATT Service Definition

### Guitar Service
- **UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f`
- **Type**: Custom 128-bit UUID
- **Purpose**: Identifies this device as a GuitarAcc guitar

### Acceleration Characteristic
- **UUID**: `a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a40`
- **Properties**: Notify only (no read/write)
- **Format**: 6 bytes [X: int16][Y: int16][Z: int16]
- **Update Rate**: Up to 10Hz when data changes

## Configuration Constants

### LED Indicators
```c
#define RUN_STATUS_LED   DK_LED1   /* LED1: Running/advertising status */
#define CON_STATUS_LED   DK_LED2   /* LED2: Connection status indicator */
```
**Theory**: Uses Nordic DK library for LED control on Thingy:53. CON_STATUS_LED provides visual feedback of BLE connection state - lit when connected to basestation, off when disconnected.

### Motion Detection
```c
#define MOTION_TIMEOUT_MS 30000    /* 30 seconds of inactivity before sleep */
#define MOTION_THRESHOLD 0.5       /* m/s² threshold for motion detection */
```
**Theory**: Device detects motion by measuring deviation from 1g gravity. If acceleration magnitude differs from 9.81 m/s² by more than 0.5 m/s², motion is detected. After 30 seconds without motion, device sleeps.

### Sampling Rate
- **Active Sampling**: 10Hz (100ms period)
- **Sleep Sampling**: 2Hz (500ms period)

**Theory**: 10Hz is sufficient for guitar motion capture while keeping BLE bandwidth reasonable. Sleep mode uses 2Hz to minimize power while still detecting wake events.

### Hardware Interrupt Configuration

The ADXL362 accelerometer is configured for hardware interrupt-driven wake-on-motion:

```properties
CONFIG_ADXL362_INTERRUPT_MODE=1         # Use INT1 pin for motion interrupts
CONFIG_ADXL362_ACTIVITY_THRESHOLD=500   # 500mg = 0.5g motion threshold
CONFIG_ADXL362_INACTIVITY_TIME=5        # 5 samples at ODR for inactivity detection
CONFIG_GPIO=y                           # GPIO subsystem for interrupt handling
```

**Theory**: Hardware activity detection allows the nRF5340 CPU to enter deep sleep (<10µA) instead of polling mode (100-500µA). The ADXL362 monitors acceleration internally and triggers a GPIO interrupt on its INT1 pin when movement exceeds the configured threshold, waking the CPU with <1ms latency.

**Note**: For detailed accelerometer wake-on-motion configuration including hardware interrupt support, see **[ACCELEROMETER.md](ACCELEROMETER.md)**.

## Test Mode

### `#define TEST_MODE_ENABLED`
**Purpose**: Development and debugging mode  
**Theory**: When enabled (set to 1), generates synthetic incrementing accelerometer data instead of reading real sensor. Useful for testing BLE connectivity and basestation integration without physical motion.

**Test Data Pattern**:
- X-axis: Increments by 10 each cycle (0, 10, 20, ... 1000, 0, ...)
- Y-axis: X + 100
- Z-axis: X + 200

**⚠️ WARNING**: Must be disabled (`#define TEST_MODE_ENABLED 0`) for production builds.

## Power Management Strategy

### Active State Power Consumption
- Accelerometer: Full power sampling at 10Hz
- BLE: Advertising or connected
- CPU: Active 100% of time

### Sleep State Power Consumption
- Accelerometer: Low-power motion detection mode
- BLE: Advertising stopped
- CPU: Active only 20% of time (500ms sleep cycles)

### Power State Transitions
```
┌─────────────┐
│   Startup   │
└──────┬──────┘
       ↓
┌─────────────┐  30s no motion      ┌─────────────┐
│    Active   │───(disconnected)───→│    Sleep    │
│   Mode      │                     │    Mode     │
│  (10Hz)     │←────────────────────│   (2Hz)     │
└──────┬──────┘  motion detected    └─────────────┘
       │
       │ BLE connect
       ↓
┌─────────────┐
│  Connected  │
│    Mode     │
│  (10Hz)     │
└──────┬──────┘
       │ disconnect
       ↓
   [back to Active]
```

## Memory Usage

From build output:
```
Flash: 121656 bytes (11.8% of 1MB)
RAM:   35768 bytes (7.0% of 512KB)
```

**Status**: Comfortable margins, ample room for feature expansion

## Known Issues and TODOs

1. ✅ ~~**TEST_MODE_ENABLED is active**~~ - **COMPLETED**: Disabled (commented out on line 40 of main.c), using real accelerometer data
2. ✅ ~~**No unit tests**~~ - **COMPLETED**: 35 comprehensive unit tests implemented in [test/test_motion.c](test/test_motion.c) covering conversion functions, magnitude calculations, motion detection, and movement threshold filtering. All tests passing (`make test` exit code 0)
3. **Motion thresholds need real-world validation** - 50 milli-g (0.05g) movement threshold for BLE transmission validated in testing logs showing motion range -718 to +938 milli-g during movement. Still needs testing with actual guitar playing. 30s inactivity timeout needs field validation
4. **Power management untested** - Sleep/wake transitions code exists but is commented out, needs hardware validation
5. **No voltage monitoring** - Consider adding battery level characteristic for user feedback
6. **⚠️ Hardware activity interrupts not functional** - GPIO interrupt handler code is implemented and INT1 pin (GPIO0.19) is properly configured, but interrupts never fire. **Root cause**: Zephyr's ADXL362 driver (v3.2.1) uses `CONFIG_ADXL362_INTERRUPT_MODE=1` for data-ready interrupts only, not activity detection. The driver does not configure ADXL362 hardware registers (INTMAP1, ACT_INACT_CTL) needed for motion-triggered interrupts. **Solution required**: Custom driver modification to write activity detection configuration to ADXL362 registers. Current polling approach (10Hz sampling) works well for guitar motion capture. Hardware interrupts would enable deep sleep mode with <10µA consumption vs current ~500µA active sampling.
7. **⚠️ RTT debug logging enabled** - SEGGER RTT enabled for development debugging (CONFIG_USE_SEGGER_RTT=y, CONFIG_LOG_BACKEND_RTT=y). **Must disable before production shipment** to improve performance and reduce attack surface. Set CONFIG_LOG=n or at minimum disable RTT backend before release.

## Build Configuration

### Project Files
- **CMakeLists.txt**: Zephyr build system configuration
- **prj.conf**: Kernel and subsystem configuration
- `CONFIG_DK_LIBRARY`: Enable Nordic DK library for LED and button support
- **Kconfig**: Project-specific Kconfig options (if any)

### Key Kconfig Options
- `CONFIG_BT`: Enable Bluetooth subsystem
- `CONFIG_BT_PERIPHERAL`: Enable BLE peripheral role
- `CONFIG_SENSOR`: Enable sensor subsystem
- `CONFIG_PM_DEVICE`: Enable device power management

## Firmware Update

The client application supports Device Firmware Update (DFU) over BLE using Nordic's MCUboot bootloader and SMP protocol. Build artifacts include:
- **merged.hex**: Complete firmware image with bootloader
- **app_update.bin**: Application-only update image for OTA
- **dfu_application.zip**: Complete DFU package for nRF Connect mobile app

## Related Documentation

- **[ACCELEROMETER.md](ACCELEROMETER.md)** - ADXL362 accelerometer configuration and API reference
- **[Basestation ARCHITECTURE.md](../basestation/ARCHITECTURE.md)** - Companion basestation application
- **[Basestation REFACTORING.md](../basestation/REFACTORING.md)** - Host-based testing framework (potential model for client tests)

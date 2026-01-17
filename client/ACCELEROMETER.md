# ADXL362 Accelerometer Configuration and Usage

## Hardware Overview

**Device**: ADXL362 3-axis MEMS Accelerometer  
**Interface**: SPI  
**Platform**: Nordic Thingy:53  
**Range**: Configurable (±2g, ±4g, ±8g) - Currently using ±2g  
**Resolution**: 12-bit  
**Power**: Ultra-low power with motion detection

## Device Tree Configuration

### Accelerometer Alias
```dts
accel0 = &adxl362;
```

The accelerometer is accessed via device tree alias, allowing hardware independence. The Thingy:53 includes an ADXL362 3-axis MEMS accelerometer connected via SPI.

## Accelerometer Access Functions (Zephyr Sensor API)

### `device_is_ready(const struct device *dev)`
**Purpose**: Verifies accelerometer device is ready for use  
**Theory**: Called during initialization to ensure device tree binding succeeded and device driver initialized properly  
**Parameters**:
- `dev`: Pointer to accelerometer device (`accel_dev` = `DEVICE_DT_GET(ACCEL_ALIAS)`)
**Returns**: true if device is ready, false otherwise

### `sensor_sample_fetch(const struct device *dev)`
**Purpose**: Triggers accelerometer to read new sample from hardware  
**Theory**: Instructs the ADXL362 driver to perform SPI transaction and fetch current XYZ acceleration values into internal buffer. Must be called before `sensor_channel_get()`  
**Parameters**:
- `dev`: Pointer to accelerometer device
**Returns**: 0 on success, negative errno on failure

### `sensor_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)`
**Purpose**: Retrieves sampled acceleration data from driver  
**Theory**: Reads buffered sensor data from previous `sensor_sample_fetch()` call. For `SENSOR_CHAN_ACCEL_XYZ`, populates array of 3 sensor_value structs (X, Y, Z) in m/s²  
**Parameters**:
- `dev`: Pointer to accelerometer device
- `chan`: Channel to read (`SENSOR_CHAN_ACCEL_XYZ` for all 3 axes)
- `val`: Array of 3 sensor_value structures to populate
**Returns**: 0 on success, negative errno on failure

### `sensor_value_to_double(const struct sensor_value *val)`
**Purpose**: Converts Zephyr sensor_value to double for calculations  
**Theory**: Sensor API uses fixed-point sensor_value struct (val1 + val2/1000000). This helper converts to floating-point for magnitude calculations and motion detection  
**Parameters**:
- `val`: Pointer to sensor_value to convert
**Returns**: double representing the sensor value in native units (m/s² for accelerometer)

### `pm_device_action_run(const struct device *dev, enum pm_device_action action)`
**Purpose**: Controls accelerometer power state  
**Theory**: Manages device power to conserve battery. `PM_DEVICE_ACTION_SUSPEND` puts ADXL362 into low-power motion detection mode. `PM_DEVICE_ACTION_RESUME` returns to full-power sampling mode  
**Parameters**:
- `dev`: Pointer to accelerometer device
- `action`: Power management action (`PM_DEVICE_ACTION_SUSPEND` or `PM_DEVICE_ACTION_RESUME`)
**Returns**: 0 on success, negative errno on failure

### Typical Usage Sequence
```c
/* Active mode sampling */
err = sensor_sample_fetch(accel_dev);           // Trigger new sample
if (err == 0) {
    sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);  // Read data
    double x = sensor_value_to_double(&accel[0]);   // Convert to double
    double y = sensor_value_to_double(&accel[1]);
    double z = sensor_value_to_double(&accel[2]);
    // Process x, y, z values...
}

/* Entering sleep mode */
pm_device_action_run(accel_dev, PM_DEVICE_ACTION_SUSPEND);

/* Waking from sleep */
pm_device_action_run(accel_dev, PM_DEVICE_ACTION_RESUME);
```

## Data Format

### Wire Format: `struct accel_data`
```c
struct accel_data {
    int16_t x;  /* X-axis in milli-g */
    int16_t y;  /* Y-axis in milli-g */
    int16_t z;  /* Z-axis in milli-g */
} __packed;
```
**Purpose**: Wire format for acceleration data transmission over BLE  
**Theory**: Uses signed 16-bit integers for ±32g range. Packed attribute ensures no padding, resulting in exactly 6 bytes on-wire

### Unit Conversion
- **Sensor Output**: m/s² (meters per second squared)
- **Transmission Format**: milli-g (thousandths of Earth's gravity)
- **Conversion**: `milli_g = (m_s2 / 9.81) * 1000.0`
- **Range**: ±2000 milli-g (±2g with current CONFIG_ADXL362_ACCEL_RANGE_2G)

## Sampling Configuration

### Sampling Rates
- **Active Mode**: 10Hz (100ms period)
- **Sleep Mode**: 2Hz (500ms period)

**Theory**: 10Hz is sufficient for guitar motion capture while keeping BLE bandwidth reasonable. Sleep mode uses 2Hz to minimize power while still detecting wake events.

### Motion Detection
```c
#define MOTION_THRESHOLD 0.5       /* m/s² threshold for motion detection */
```
**Theory**: Device detects motion by measuring deviation from 1g gravity. If acceleration magnitude differs from 9.81 m/s² by more than 0.5 m/s², motion is detected.

**Algorithm**:
```c
double magnitude = sqrt(x*x + y*y + z*z);
if (fabs(magnitude - 9.81) > MOTION_THRESHOLD) {
    // Motion detected
}
```

## Wake-on-Motion Configuration

### Current Implementation: Software Polling
The current implementation uses **software-based motion detection** in sleep mode:
- CPU wakes every 500ms to sample accelerometer
- Calculates magnitude and compares to `MOTION_THRESHOLD` (0.5 m/s²)
- If motion detected, calls `wake_from_motion()` to resume normal operation

**Limitation**: CPU must remain active to poll sensor, limiting power savings.

### Hardware Wake Support (Not Currently Used)

The ADXL362 accelerometer has built-in motion detection hardware that can generate interrupts, enabling true low-power wake-on-motion:

#### Available ADXL362 Hardware Features
1. **Activity Detection**: Hardware monitors for motion above configurable threshold
2. **Inactivity Detection**: Hardware monitors for periods of no motion
3. **GPIO Interrupt Output**: Can trigger nRF5340 GPIO interrupt without CPU polling

#### ADXL362 Kconfig Options (Available but Not Configured)
```properties
# Not currently enabled in prj.conf:
CONFIG_ADXL362_INTERRUPT_MODE=0      # 0=polling, 1=INT1 pin, 2=INT2 pin
CONFIG_ADXL362_ACTIVITY_THRESHOLD    # Activity detection threshold (mg)
CONFIG_ADXL362_INACTIVITY_THRESHOLD  # Inactivity detection threshold (mg)
CONFIG_ADXL362_INACTIVITY_TIME       # Time for inactivity detection (samples)
```

**Default Values** (from build system):
- `CONFIG_ADXL362_ACTIVITY_THRESHOLD = 1000` (1000mg = 1g)
- `CONFIG_ADXL362_INACTIVITY_THRESHOLD = 100` (100mg = 0.1g)

#### Enabling Hardware Wake-on-Motion

To enable interrupt-driven wake from sleep, modify **prj.conf**:

```properties
# Enable ADXL362 interrupt-driven mode
CONFIG_ADXL362_INTERRUPT_MODE=1            # Use INT1 pin
CONFIG_ADXL362_ACTIVITY_THRESHOLD=500      # 500mg = 0.5g motion threshold
CONFIG_ADXL362_INACTIVITY_TIME=5           # 5 samples at ODR for inactivity

# Enable GPIO interrupt for wake
CONFIG_GPIO=y
```

Then configure interrupt handling in code:
```c
/* In initialization: */
static struct gpio_callback accel_cb_data;

static void accel_interrupt_handler(const struct device *dev,
                                    struct gpio_callback *cb,
                                    uint32_t pins)
{
    /* Motion detected - wake from sleep */
    wake_from_motion();
}

/* Setup GPIO interrupt from ADXL362 INT1 pin */
// Device tree overlay needed to map INT1 pin to GPIO
// Then: gpio_init_callback(), gpio_add_callback(), gpio_pin_interrupt_configure()
```

**Benefits of Hardware Wake**:
- CPU can enter deep sleep (significant power savings)
- Faster wake response (no 500ms polling delay)
- More accurate threshold detection (no floating-point calculations)
- Lower overall power consumption

**Status**: ⚠️ **TODO** - Currently not implemented. System uses software polling approach.

### Software vs Hardware Wake Comparison

| Feature | Software Polling (Current) | Hardware Interrupt (Future) |
|---------|---------------------------|------------------------------|
| Power Mode | Light sleep (CPU active every 500ms) | Deep sleep (CPU off until interrupt) |
| Wake Latency | Up to 500ms | <1ms |
| Power Consumption | ~100-500µA | <10µA |
| Threshold Precision | Floating-point calculation | Hardware comparator |
| Implementation | Simple, no device tree changes | Requires GPIO interrupt configuration |
| Status | ✅ Implemented | ⚠️ TODO |

## Current Configuration (prj.conf)

```properties
# ADXL362 Accelerometer Configuration
CONFIG_SENSOR=y
CONFIG_ADXL362=y
CONFIG_ADXL362_ACCEL_RANGE_2G=y          # ±2g measurement range
CONFIG_ADXL362_INTERRUPT_MODE=0          # Polling mode (not interrupt-driven)
CONFIG_ADXL362_ABS_REF_MODE=0            # Relative reference mode
```

## Power Management

### Power States
1. **Active Sampling (Full Power)**
   - Continuous sampling at configured Output Data Rate (ODR)
   - SPI interface active
   - ~1.8µA at 100Hz ODR

2. **Suspend Mode (Low Power)**
   - Enabled via `pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND)`
   - Motion detection active (if configured)
   - ~270nA standby current

3. **Resume Mode**
   - Returns to full power sampling
   - Enabled via `pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME)`

## Known Issues and Improvements

1. **Software polling in sleep mode** - Not using ADXL362 hardware interrupt capabilities, limiting power savings. Should implement interrupt-driven wake-on-motion
2. **Motion thresholds untested** - 0.5 m/s² threshold needs validation with real guitar usage
3. **Fixed sampling rate** - ODR not dynamically adjusted based on motion intensity
4. **No self-test** - ADXL362 has built-in self-test capability that could verify sensor function on startup
5. **No FIFO usage** - ADXL362 has 512-sample FIFO that could reduce CPU wake frequency

## Related Documentation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** - Client application system architecture
- **[Zephyr ADXL362 Driver](https://docs.zephyrproject.org/latest/reference/peripherals/sensor.html)** - Zephyr sensor API documentation
- **[ADXL362 Datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ADXL362.pdf)** - Hardware specifications and capabilities

# Client Filter Features

This document describes the two new signal processing features added to the client application.

## Overview

Two new filtering features have been added to improve accelerometer data quality:

1. **Spike Limiter** - Prevents sudden large changes in acceleration values
2. **Running Average Filter** - Smooths accelerometer data using a configurable averaging window

Both filters work on each axis (X, Y, Z) independently and are applied in sequence: spike limiter first, then running average.

## Feature 1: Spike Limiter

### Purpose
The spike limiter prevents sudden, unrealistic spikes in acceleration data that may result from sensor noise, electrical interference, or sensor errors.

### Configuration
Located in [motion_logic.h](src/motion_logic.h):

```c
#define SPIKE_LIMIT_MILLI_G 500  /* Maximum allowed change per sample (0.5g) */
```

### How It Works
- Tracks the previous acceleration value for each axis
- Limits the change between consecutive samples to ±`SPIKE_LIMIT_MILLI_G`
- If a new reading differs by more than the limit, it is clamped to the maximum allowed change
- Example: If previous X = 1000 and new X = 2000 (change of 1000), it will be clamped to 1500 (change of 500)

### API Functions

```c
/* Initialize with starting values */
void spike_limiter_init(const struct accel_data *initial);

/* Apply spike limiting to raw data */
void apply_spike_limiter(const struct accel_data *raw, struct accel_data *limited);
```

### Usage in main.c
```c
/* At startup */
spike_limiter_init(NULL);  /* Initialize with zeros */

/* In main loop */
struct accel_data raw_accel, filtered_accel;
convert_accel_to_milli_g(x, y, z, &raw_accel);
apply_spike_limiter(&raw_accel, &filtered_accel);
```

## Feature 2: Running Average Filter

### Purpose
The running average filter smooths accelerometer data by averaging the last N samples for each axis. This reduces high-frequency noise while preserving the overall motion trend.

### Configuration
Located in [motion_logic.h](src/motion_logic.h):

```c
/* Enable/disable running average filter */
#define ENABLE_RUNNING_AVERAGE 1

/* Running average depth (adjustable 3-10) */
#define RUNNING_AVERAGE_DEPTH 5
```

**Note:** The depth is automatically clamped to the range [3, 10] in the implementation.

### How It Works
- Maintains circular buffers for X, Y, and Z axes
- Each buffer stores the last `RUNNING_AVERAGE_DEPTH` samples
- Output is the arithmetic mean of all samples in the buffer
- Buffer fills gradually from empty, so early samples use fewer values for averaging

### Enabling/Disabling
To **disable** the running average filter:
```c
#define ENABLE_RUNNING_AVERAGE 0
```
or comment out the line:
```c
// #define ENABLE_RUNNING_AVERAGE 1
```

When disabled, the spike-limited data is used directly without averaging.

### API Functions

```c
/* Initialize filter (resets buffers) */
void running_average_init(void);

/* Apply running average smoothing */
void apply_running_average(const struct accel_data *input, struct accel_data *output);
```

### Usage in main.c
```c
/* At startup */
#if ENABLE_RUNNING_AVERAGE
running_average_init();
#endif

/* In main loop */
#if ENABLE_RUNNING_AVERAGE
apply_running_average(&filtered_accel, &current_accel);
#else
current_accel = filtered_accel;  /* Use spike-limited data directly */
#endif
```

## Data Flow Pipeline

The complete signal processing pipeline in the client application:

```
Sensor Reading (m/s²)
       ↓
Convert to milli-g
       ↓
Spike Limiter (limit sudden changes)
       ↓
Running Average (if enabled)
       ↓
BLE Transmission
```

## Configuration Examples

### Conservative Filtering (More Smoothing)
```c
#define SPIKE_LIMIT_MILLI_G 300        // Tighter spike limit
#define ENABLE_RUNNING_AVERAGE 1
#define RUNNING_AVERAGE_DEPTH 10       // Maximum depth
```

### Aggressive Filtering (Less Latency)
```c
#define SPIKE_LIMIT_MILLI_G 500        // Default spike limit
#define ENABLE_RUNNING_AVERAGE 1
#define RUNNING_AVERAGE_DEPTH 3        // Minimum depth
```

### Spike Limiter Only (No Averaging)
```c
#define SPIKE_LIMIT_MILLI_G 500
#define ENABLE_RUNNING_AVERAGE 0       // Disabled
```

### No Filtering (Raw Data)
To completely disable filtering, comment out the filter applications in [main.c](src/main.c) and use the raw converted data directly.

## Testing

A comprehensive test suite is provided in [test/test_filters.c](test/test_filters.c).

### Running Tests
```bash
cd client/test
make test
```

### Test Coverage
- Spike limiter basic functionality
- Large spike limiting (positive and negative)
- Running average initialization and smoothing
- Combined filter operation (spike limiter + running average)
- Edge cases (NULL handling, INT16 limits)

### Example Test Output
```
Configuration:
  SPIKE_LIMIT_MILLI_G: 500
  ENABLE_RUNNING_AVERAGE: 1
  RUNNING_AVERAGE_DEPTH: 5

Test: Spike Limiter - Basic Functionality
  ✓ Small changes pass through correctly
  ✓ Large spikes are limited correctly
  ✓ Negative spikes are limited correctly

Test: Running Average - Smoothing Effect
  Input sequence with spike: 1000 1010 990 2000 1005 995 1000
  Smoothed output: 1000 1005 1000 1250 1201 1200 1198
  ✓ Spike is smoothed by averaging

✓ ALL TESTS PASSED!
```

## Performance Considerations

### Memory Usage
- **Spike Limiter**: 6 bytes (one `struct accel_data` for previous values)
- **Running Average**: 6 × `RUNNING_AVERAGE_DEPTH` bytes (three int16 buffers)
  - Depth 3: 18 bytes
  - Depth 5: 30 bytes
  - Depth 10: 60 bytes

### CPU Usage
- **Spike Limiter**: O(1) - three comparisons and clamps per sample
- **Running Average**: O(N) where N = `RUNNING_AVERAGE_DEPTH`
  - Depth 5: 15 additions per sample
  - Both filters easily run within the 100ms sample period

### Latency
- **Spike Limiter**: Zero latency (instant response)
- **Running Average**: Group delay of `(RUNNING_AVERAGE_DEPTH - 1) / 2` samples
  - Depth 5: ~2 sample delay (~200ms at 10Hz)
  - Depth 10: ~4.5 sample delay (~450ms at 10Hz)

## Troubleshooting

### High Noise Despite Filtering
- Increase `RUNNING_AVERAGE_DEPTH` (up to 10)
- Decrease `SPIKE_LIMIT_MILLI_G` for tighter limiting

### Excessive Latency
- Decrease `RUNNING_AVERAGE_DEPTH` (down to 3)
- Consider disabling running average entirely

### Unexpected Values
- Check initialization: ensure both filters are initialized at startup
- Verify `SPIKE_LIMIT_MILLI_G` is reasonable (100-1000 range typical)
- Run test suite to validate filter behavior

## Integration Notes

### Initialization Order
1. Configure accelerometer sensor
2. Initialize spike limiter: `spike_limiter_init(NULL)`
3. Initialize running average: `running_average_init()` (if enabled)
4. Start BLE advertising
5. Enter main loop

### Filter Order
Always apply filters in this order:
1. Spike limiter (removes large transients)
2. Running average (smooths remaining noise)

Reversing the order would allow spikes to contaminate multiple averaged samples.

## Files Modified

### Header Files
- [src/motion_logic.h](src/motion_logic.h) - Added filter configuration and API declarations

### Source Files
- [src/motion_logic.c](src/motion_logic.c) - Added spike limiter and running average implementations
- [src/main.c](src/main.c) - Integrated filters into main accelerometer processing loop

### Test Files
- [test/test_filters.c](test/test_filters.c) - Comprehensive test suite for new features
- [test/Makefile](test/Makefile) - Updated to build and run filter tests

## References

- Main accelerometer processing: [src/main.c](src/main.c) lines 538-566
- Filter definitions: [src/motion_logic.h](src/motion_logic.h) lines 19-61
- Filter implementations: [src/motion_logic.c](src/motion_logic.c) lines 18-131

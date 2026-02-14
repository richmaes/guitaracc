# Accelerometer to MIDI Value Mapping

## Overview

The GuitarAcc system now includes a programmable abstraction layer for translating accelerometer data to MIDI values. This allows for flexible configuration of how motion translates to musical control.

## Architecture

### Files
- [`src/accel_mapping.h`](src/accel_mapping.h) - Mapping interface and configuration structures
- [`src/accel_mapping.c`](src/accel_mapping.c) - Core mapping implementation
- [`src/midi_logic.h`](src/midi_logic.h) - Updated MIDI logic to use configurable mapping
- [`src/midi_logic.c`](src/midi_logic.c) - Integration with mapping layer
- [`test/test_accel_mapping.c`](test/test_accel_mapping.c) - Comprehensive unit tests

### Data Flow

```
Accelerometer     Mapping           MIDI
  (milli-g)    Configuration       Value
     ↓              ↓                ↓
  ┌─────┐      ┌─────────┐      ┌─────┐
  │-2000│  →   │Linear   │  →   │  0  │
  │  0  │  →   │Mapping  │  →   │ 63  │
  │+2000│  →   │(-2000   │  →   │ 127 │
  └─────┘      │to +2000)│      └─────┘
               └─────────┘
```

## Current Implementation: Linear Mapping

The current implementation provides a linear mapping with two configurable points:

### Configuration Structure

```c
struct accel_mapping_config {
    int16_t accel_min;  /* Accelerometer value that maps to MIDI 0 */
    int16_t accel_max;  /* Accelerometer value that maps to MIDI 127 */
};
```

### Mapping Formula

For a given accelerometer value `accel_value`:

```
midi_value = (accel_value - accel_min) × 127 / (accel_max - accel_min)
```

Values outside the configured range are clamped to 0 or 127.

### Default Configuration

The default mapping uses a ±2g range:
- `-2000 mg` → MIDI 0
- `0 mg` → MIDI 63 (center)
- `+2000 mg` → MIDI 127

## Usage Examples

### Using Default Mapping

```c
#include "midi_logic.h"

/* Convert accelerometer value using default mapping */
uint8_t midi_val = accel_to_midi_cc(1000, NULL);  /* NULL = use default */
```

### Custom Mapping Configuration

```c
#include "accel_mapping.h"
#include "midi_logic.h"

/* Create a custom mapping: -1000mg to +1000mg (±1g range) */
struct accel_mapping_config custom_config;
accel_mapping_init_linear(&custom_config, -1000, 1000);

/* Use the custom mapping */
uint8_t midi_val = accel_to_midi_cc(500, &custom_config);
```

### Positive Range Only

```c
/* Map only positive acceleration: 0mg to +2000mg */
struct accel_mapping_config positive_config;
accel_mapping_init_linear(&positive_config, 0, 2000);

uint8_t midi_val = accel_to_midi_cc(1000, &positive_config);
/* 0mg → 0, 1000mg → 63, 2000mg → 127 */
```

### Inverted Mapping

```c
/* Invert the mapping: high acceleration → low MIDI values */
struct accel_mapping_config inverted_config;
accel_mapping_init_linear(&inverted_config, 2000, -2000);

uint8_t midi_val = accel_to_midi_cc(1000, &inverted_config);
/* +2000mg → 0, 0mg → 63, -2000mg → 127 */
```

## API Reference

### `accel_map_to_midi()`

Maps an accelerometer value to a MIDI value using the provided configuration.

```c
uint8_t accel_map_to_midi(const struct accel_mapping_config *config, 
                          int16_t accel_value);
```

**Parameters:**
- `config` - Pointer to mapping configuration (returns 0 if NULL)
- `accel_value` - Accelerometer reading in milli-g

**Returns:** MIDI value (0-127), clamped to valid range

### `accel_mapping_init_linear()`

Initializes a linear mapping configuration.

```c
void accel_mapping_init_linear(struct accel_mapping_config *config, 
                                int16_t accel_min, 
                                int16_t accel_max);
```

**Parameters:**
- `config` - Pointer to configuration structure to initialize
- `accel_min` - Accelerometer value that maps to MIDI 0
- `accel_max` - Accelerometer value that maps to MIDI 127

### `accel_to_midi_cc()`

Converts accelerometer value to MIDI CC value (wrapper function).

```c
uint8_t accel_to_midi_cc(int16_t milli_g, 
                         const struct accel_mapping_config *config);
```

**Parameters:**
- `milli_g` - Acceleration in milli-g
- `config` - Mapping configuration (NULL uses default)

**Returns:** MIDI CC value (0-127)

### `get_default_accel_mapping()`

Returns the default mapping configuration.

```c
const struct accel_mapping_config *get_default_accel_mapping(void);
```

**Returns:** Pointer to default configuration (±2g range)

## Testing

Comprehensive unit tests are provided in `test/test_accel_mapping.c`:

```bash
cd basestation/test
make test
```

Tests cover:
- Default linear mapping (±2g)
- Custom range configurations
- Positive and negative range mappings
- Inverted mappings
- Narrow range mappings
- Edge cases (NULL, zero range)
- MIDI boundary precision

All 51 tests currently pass.

## Future Enhancements

### Planned Features

1. **UI Configuration** - The abstraction layer is designed to receive configuration from a user interface (future work)

2. **Additional Mapping Types**
   - Exponential curves for more responsive control
   - Logarithmic curves for fine control at low values
   - Custom curve definitions

3. **Per-Axis Configuration**
   - Different mappings for X, Y, Z axes
   - Axis-specific calibration

4. **Dynamic Range Adjustment**
   - Auto-calibration based on observed motion
   - User-triggered calibration mode

### Structure Ready for Extension

The `accel_mapping_config` structure can be extended:

```c
struct accel_mapping_config {
    /* Current linear mapping */
    int16_t accel_min;
    int16_t accel_max;
    
    /* Future additions */
    // enum mapping_type { LINEAR, EXPONENTIAL, LOGARITHMIC, CUSTOM };
    // mapping_type type;
    // float curve_parameter;
    // uint8_t *custom_lut;  /* Lookup table for custom curves */
};
```

## Implementation Notes

### Integer Arithmetic

The mapping uses integer arithmetic to avoid floating-point operations on the embedded target:

```c
int32_t accel_range = (int32_t)config->accel_max - (int32_t)config->accel_min;
int32_t accel_offset = (int32_t)accel_value - (int32_t)config->accel_min;
int32_t midi_value = (accel_offset * 127) / accel_range;
```

This ensures efficient execution on the nRF5340 without requiring FPU operations.

### Clamping

Values outside the configured range are automatically clamped:
- Below `accel_min` → MIDI 0
- Above `accel_max` → MIDI 127

This provides safe behavior even with unexpected sensor readings.

### Division by Zero Protection

If `accel_min == accel_max`, the function returns MIDI 0 to avoid division by zero.

## Related Documentation

- [ARCHITECTURE.md](../ARCHITECTURE.md) - System overview
- [basestation/REFACTORING.md](REFACTORING.md) - Testing approach
- [MIDI_SPEC.md](MIDI_SPEC.md) - MIDI message format details

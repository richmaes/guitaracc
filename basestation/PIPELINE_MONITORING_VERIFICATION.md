# Pipeline Monitoring Feature Verification

## Implementation Summary

### 1. Monitoring Commands

#### `pipeline_monitor enable`
Starts continuous monitoring in human-readable format.

**Output Format:**
```
[timestamp_ms ms]
  Raw Axis (milli-g):       X=value  Y=value  Z=value
  Input Vector (g):         X=value Y=value Z=value
  Rotated Vector (g):       X=value Y=value Z=value
  Normalized Unit Vector:   X=value Y=value Z=value
  Scalar Projection (X):    value
  Function: [Linear|Exponential|S-Curve|Lookup]
  MIDI Output:              CC number = value
```

#### `pipeline_monitor json`
Starts continuous monitoring in JSON format (one object per line).

**Output Format:**
```json
{"timestamp_ms":12345,"raw_axis":{"x":100,"y":200,"z":300},"input_vector":{"x":0.100,"y":0.200,"z":0.300},"rotated_vector":{"x":0.150,"y":0.180,"z":0.290},"normalized_vector":{"x":0.400,"y":0.480,"z":0.773},"scalar_projection":0.400,"function_type":"linear","midi_output":{"cc":1,"value":64}}
```

#### `pipeline_monitor disable`
Stops monitoring output.

### 2. Configuration Commands

#### `pipeline show`
Displays current pipeline configuration in human-readable format.

**Output:**
```
=== Accelerometer Rotation Pipeline (Patch 0) ===

Rotation:
  Rho (X-axis):   45.0 degrees
  Theta (Y-axis): 90.0 degrees

Output:
  MIDI CC:        1

Conversion Function: Linear
  Scale:  1.00
  Offset: 0.00
```

#### `pipeline json`
Displays current pipeline configuration in JSON format.

**Output:**
```json
{
  "patch": 0,
  "rotation": {
    "rho_degrees": 45.0,
    "theta_degrees": 90.0
  },
  "output": {
    "midi_cc": 1
  },
  "conversion": {
    "function_type": "linear",
    "parameters": {
      "scale": 1.00,
      "offset": 0.00
    }
  }
}
```

## Field Verification Checklist

### Monitoring Output Fields

**Human-Readable Format:**
- [x] timestamp_ms - Time in milliseconds since boot
- [x] raw_axis.x - Raw X-axis accelerometer value (milli-g)
- [x] raw_axis.y - Raw Y-axis accelerometer value (milli-g)
- [x] raw_axis.z - Raw Z-axis accelerometer value (milli-g)
- [x] input_vector.x - Input vector X component (g)
- [x] input_vector.y - Input vector Y component (g)
- [x] input_vector.z - Input vector Z component (g)
- [x] rotated_vector.x - After rotation, X component (g)
- [x] rotated_vector.y - After rotation, Y component (g)
- [x] rotated_vector.z - After rotation, Z component (g)
- [x] normalized_vector.x - Normalized unit vector X
- [x] normalized_vector.y - Normalized unit vector Y
- [x] normalized_vector.z - Normalized unit vector Z
- [x] scalar_projection - X-axis projection value
- [x] function_type - Conversion function name
- [x] midi_output.cc - MIDI CC number
- [x] midi_output.value - MIDI CC value (0-127)

**JSON Format:**
- [x] timestamp_ms
- [x] raw_axis {x, y, z}
- [x] input_vector {x, y, z}
- [x] rotated_vector {x, y, z}
- [x] normalized_vector {x, y, z}
- [x] scalar_projection
- [x] function_type
- [x] midi_output {cc, value}

### Configuration Output Fields

**Human-Readable Format (pipeline show):**
- [x] patch - Active patch number
- [x] rotation.rho - X-axis rotation angle (degrees)
- [x] rotation.theta - Y-axis rotation angle (degrees)
- [x] output.midi_cc - MIDI CC number
- [x] conversion.function_type - Function name
- [x] conversion.parameters - Function-specific parameters

**JSON Format (pipeline json):**
- [x] patch
- [x] rotation {rho_degrees, theta_degrees}
- [x] output {midi_cc}
- [x] conversion {function_type, parameters}

## Configuration Requirements

### prj.conf
Floating-point printf support is enabled:
```
CONFIG_CBPRINTF_FP_SUPPORT=y
```

This is required for displaying float values in both monitoring and configuration output.

## Testing Commands

### Test Human-Readable Monitoring
```
pipeline_monitor enable
# Wait for accelerometer data
# Press Ctrl+C to stop
pipeline_monitor disable
```

### Test JSON Monitoring
```
pipeline_monitor json
# Wait for accelerometer data
# Press Ctrl+C to stop
pipeline_monitor disable
```

### Test Configuration Display
```
pipeline show
pipeline json
```

### Test Complete Workflow
```
# Configure pipeline
pipeline set 45 90 1 linear 1.0 0.0

# View configuration
pipeline show
pipeline json

# Monitor in human format
pipeline_monitor enable
# ... observe output ...
pipeline_monitor disable

# Monitor in JSON format
pipeline_monitor json
# ... observe output ...
pipeline_monitor disable
```

## Implementation Files

- **basestation/src/main.c**: Monitoring output logic
- **basestation/src/ui_interface.h**: API declarations and enum
- **basestation/src/ui_interface_shell.c**: Shell commands
- **basestation/prj.conf**: Float printf support configuration

## Notes

All intermediate pipeline values are captured and displayed:
1. **Raw Axis Data** - Original accelerometer values in milli-g
2. **Input Vector** - Converted to g units
3. **Rotated Vector** - After angular transformation
4. **Normalized Vector** - Converted to unit vector
5. **Scalar Projection** - X-axis component for MIDI conversion
6. **Function Output** - Final MIDI CC value
7. **MIDI Output** - CC number and value sent

Both formats provide identical information, just in different presentation styles for different use cases (human debugging vs. machine parsing).

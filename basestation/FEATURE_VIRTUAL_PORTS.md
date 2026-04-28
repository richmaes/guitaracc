# Feature: Accelerometer to MIDI CC Abstraction Mapping

## Status
**Planning** - Architectural design phase

## Overview

This feature introduces a flexible signal processing architecture using **virtual ports** and **function abstraction units** to enable complex accelerometer-to-MIDI mappings beyond simple linear scaling.

## High-Level Architecture

### Core Concepts

1. **Function Abstraction Units** (fixed quantity)
   - Programmable signal processing blocks
   - Each unit has configurable inputs and outputs
   - Function types defined separately (TBD)

2. **Virtual Ports** (signal routing fabric)
   - Named connection points for signal routing
   - Act as mixers when multiple outputs connect to same port
   - Can connect to multiple inputs (fan-out)

3. **Signal Sources**
   - Accelerometer outputs (X, Y, Z axes + derived values)
   - Virtual port outputs (function unit outputs)

4. **Signal Destinations**
   - Virtual port inputs (function unit inputs)
   - MIDI CC processor (final output stage)

### Signal Flow (Layered Architecture)

```
┌─────────────────────────────────────────────────────────┐
│  HARDWARE LAYER (Shared Physical Resources)             │
│  ┌──────────────────┐      ┌──────────────────┐        │
│  │ Accelerometer    │      │ Gyroscope        │        │
│  │ X, Y, Z (raw)    │      │ Roll, Pitch, Yaw │        │
│  │ (int16_t mg)     │      │ (raw)            │        │
│  └────────┬─────────┘      └────────┬─────────┘        │
└───────────┼────────────────────────┼──────────────────┘
            │                        │
            v                        v
┌─────────────────────────────────────────────────────────┐
│  CALIBRATION LAYER (Shared Global Resources)            │
│  ┌──────────────────────────────────────────────────┐  │
│  │  Global Scale/Offset Function                    │  │
│  │  - Applies config.accel_scale[0..5]              │  │
│  │  - Applies config.accel_offset[0..5]             │  │
│  │  - Converts raw mg values to calibrated range    │  │
│  │  (FUNC_SCALE_OFFSET type, shared resource)       │  │
│  └──────────────────┬───────────────────────────────┘  │
└─────────────────────┼──────────────────────────────────┘
                      │ Calibrated sensor values
                      v
┌─────────────────────────────────────────────────────────┐
│  TOPOLOGY LAYER (Per-Patch Signal Processing)           │
│                                                          │
│      Virtual Port Fabric & Function Units               │
│  ┌──────────┐  ┌──────────┐                            │
│  │ VP[0]    │  │ VP[1]    │  ...                       │
│  │ (mixer)  │  │ (mixer)  │                            │
│  └────┬─────┘  └────┬─────┘                            │
│       │             │                                   │
│       v             v                                   │
│  ┌─────────┐  ┌─────────┐                              │
│  │ Func[0] │  │ Func[1] │  ...                         │
│  │ (LINEAR,│  │ (CURVE, │                              │
│  │  etc.)  │  │  etc.)  │                              │
│  └────┬────┘  └────┬────┘                              │
│       │            │                                    │
│       v            v                                    │
│  ┌──────────┐  ┌──────────┐                            │
│  │ VP[N]    │  │ VP[N+1]  │  ...                       │
│  │ (mixer)  │  │ (mixer)  │                            │
│  └────┬─────┘  └────┬─────┘                            │
└───────┼─────────────┼──────────────────────────────────┘
        │             │
        v             v
   ┌─────────────────────┐
   │  MIDI CC Processor  │
   │  (CC mapping)       │
   └─────────────────────┘
             │
             v
       MIDI Output
```

### Architecture Layers

**Hardware Layer**: Physical sensors provide raw readings. These are shared resources - all patches see the same sensor data.
- Accelerometer: X, Y, Z axes (raw milli-g values)
- Gyroscope: Roll, Pitch, Yaw (raw values)

**Calibration Layer**: Global scale/offset transformation applies hardware calibration stored in `config.global.accel_scale[]` and `config.global.accel_offset[]`. This is a shared resource - one calibration applies to all patches.
- Converts raw sensor readings to calibrated working range
- Uses FUNC_SCALE_OFFSET function type
- Configuration persisted in global settings

**Topology Layer**: Per-patch signal routing and processing. Each patch can have different topology configurations.
- Virtual ports provide signal routing fabric
- Function units perform per-patch transformations (LINEAR, CURVE, etc.)
- Multiple topology instances can run concurrently

## Shared Resources

### Sensor Sources (Hardware Layer)

Sensor sources are **shared physical resources** that provide raw readings from hardware:

```c
// Sensor source identifiers (0-5)
enum sensor_source {
    SENSOR_ACCEL_X = 0,      // Accelerometer X-axis (raw mg)
    SENSOR_ACCEL_Y = 1,      // Accelerometer Y-axis (raw mg)
    SENSOR_ACCEL_Z = 2,      // Accelerometer Z-axis (raw mg)
    SENSOR_GYRO_ROLL = 3,    // Gyroscope Roll (raw)
    SENSOR_GYRO_PITCH = 4,   // Gyroscope Pitch (raw)
    SENSOR_GYRO_YAW = 5,     // Gyroscope Yaw (raw)
};
```

**Properties**:
- Read-only - all patches see the same sensor values
- Updated at sensor sampling rate (typically 100-200 Hz)
- Values are raw hardware readings (e.g., milli-g for accelerometer)
- No patch-specific filtering or transformation

**Usage**: Sensor sources are referenced by topology instances via `accel_inputs[]` field.

### Global Scale/Offset Function (Calibration Layer)

The global scale/offset function is a **shared calibration resource** that transforms raw sensor readings into a calibrated working range:

```c
// Global scale/offset configuration (stored in config.global)
struct global_config {
    // ... other global settings ...
    
    // Scale/offset per sensor (6 sensors)
    int16_t accel_scale[6];   // Full-scale range in milli-g (100-4000mg)
    int16_t accel_offset[6];  // Center point offset in milli-g
};
```

**Purpose**:
- Hardware calibration - compensate for sensor mounting angle, drift, etc.
- User preference - adjust sensitivity globally across all patches
- Consistent behavior - same calibration applies to all patches

**Function Type**: `FUNC_SCALE_OFFSET`
- Input: Raw sensor value (int16_t, in milli-g)
- Output: Calibrated value mapped to working range
- Parameters: Read from global configuration (not per-function)

**Example Calibration**:
```c
// X-axis: ±1g (1000mg) maps to MIDI 0-127
accel_scale[0] = 1000;
accel_offset[0] = 0;    // Neutral position at 0g

// Y-axis: ±2g (2000mg) maps to MIDI 0-127, offset by +200mg
accel_scale[1] = 2000;
accel_offset[1] = 200;  // Neutral position at +0.2g
```

**Processing Formula**:
```
calibrated = ((raw - offset) * 127) / scale
calibrated = CLAMP(calibrated, -32768, 32767)  // Keep as int16_t
```

**Shared Resource Properties**:
- Single calibration applies to all patches
- Cannot be overridden per-patch
- Configuration stored in global settings (persists across power cycles)
- Changes affect all active topologies immediately

**Implementation Note**: The global scale/offset function can be implemented as:
1. **Pre-processing**: Applied once before topology processing (recommended)
2. **Function Unit**: Special FUNC_SCALE_OFFSET type that reads global config
3. **Implicit**: Built into sensor read operations

**Recommended Approach**: Pre-processing - apply calibration once per sensor reading before topology processing for efficiency.

## Virtual Port Architecture

### Virtual Port Definition

A **virtual port** is a signal connection point with the following properties:

- **Input Mixing**: Multiple signal sources can write to a virtual port
  - Mixing algorithm: TBD (sum, average, weighted, etc.)
  - Overflow handling: Clamp to valid MIDI range (0-127)
  
- **Fan-out**: Single virtual port can drive multiple destinations
  - All connected inputs read the same mixed value
  - No signal degradation with multiple readers

- **Port Types** (optional classification):
  - General purpose (any signal type)
  - OR type-specific (accelerometer, MIDI value, etc.)

### Port Numbering/Naming

```
VP[0..N]  - Virtual ports (fixed array)
```

**Consideration**: Should ports be numbered or named?
- **Numbered**: `VP[0]`, `VP[1]`, ... `VP[N]` (simpler implementation)
- **Named**: `"accel_x_raw"`, `"filter_out"`, etc. (better readability)
- **Hybrid**: Numbered with optional user-assigned names

**Decision**: TBD

## Connection Model

### Source → Virtual Port Connections

Multiple sources can connect to a single virtual port (mixing):

```
Accel_X  ──┐
           ├──→ VP[0] (mixer)
Func[0]  ──┘
```

**Configuration Data**:
```c
struct vport_input_connection {
    uint8_t source_type;  // ACCEL, FUNC_UNIT, CONSTANT
    uint8_t source_id;    // Which accel axis, which function unit
    uint8_t vport_num;    // Destination virtual port
    uint8_t weight;       // Mixing weight (optional, for weighted avg)
};
```

### Virtual Port → Destination Connections

A virtual port can connect to multiple function unit inputs (fan-out):

```
              ┌──→ Func[0].input[0]
VP[0] ────────┼──→ Func[1].input[0]
              └──→ MIDI_CC[16]
```

**Configuration Data**:
```c
struct vport_output_connection {
    uint8_t vport_num;       // Source virtual port
    uint8_t dest_type;       // FUNC_UNIT, MIDI_CC
    uint8_t dest_id;         // Which function unit or CC number
    uint8_t dest_input;      // Which input on function unit (if applicable)
};
```

### Special Connections

#### Direct Accelerometer → MIDI CC
Allow bypass of function units for simple mappings:
```
Accel_X ──→ VP[0] ──→ MIDI_CC[16]
```

#### Function Chaining
Function units can be chained through virtual ports:
```
VP[0] ──→ Func[0] ──→ VP[1] ──→ Func[1] ──→ VP[2] ──→ MIDI_CC[16]
```

#### Feedback (Future Consideration)
```
VP[0] ──→ Func[0] ──→ VP[1] ──┐
            ↑                  │
            └──────────────────┘
```
**Note**: Feedback loops require careful handling (latency, stability)

## Resource Allocation

### Fixed Resource Limits

To enable efficient embedded implementation, use fixed resource allocation:

```c
#define MAX_VIRTUAL_PORTS       16   // Virtual port array size
#define MAX_FUNCTION_UNITS      8    // Function processing blocks
#define MAX_ACCEL_SOURCES       6    // X, Y, Z, Roll, Pitch, Yaw
#define MAX_MIDI_OUTPUTS        6    // Configurable CC numbers per patch
#define MAX_TOPOLOGY_INSTANCES  6    // Concurrent topology instances per patch
#define NUM_TOPOLOGY_TYPES      4    // T1, T2, T3, T4 (expandable to 15)
```

**Resource Usage by Topology Type**:
- **T1**: 2 VPs, 1 function unit, 1 accel source, 1 MIDI output
- **T2**: 2 VPs, 1 function unit, 2 accel sources, 1 MIDI output
- **T3**: 2 VPs, 1 function unit, 1 accel source, 2 MIDI outputs
- **T4**: 3 VPs, 2 function units, 2 accel sources, 2 MIDI outputs

**Practical Configuration**: 6 concurrent T1 instances (one per accel axis) is the common case.

**Design Decision**: Fixed limits provide:
- Predictable memory usage
- Fast processing (no dynamic allocation)
- Simple configuration validation

## Virtual Port Mixer Algorithm

When multiple sources write to a virtual port, a mixing algorithm combines them:

### Option 1: Summation (Clamped)
```c
int16_t vport_value = 0;
for (each connected source) {
    vport_value += source_value;
}
vport_value = CLAMP(vport_value, 0, 127);  // Clamp to MIDI range
```

**Pros**: Simple, preserves signal energy
**Cons**: Easy to saturate/clip

### Option 2: Averaging
```c
int16_t sum = 0;
uint8_t count = 0;
for (each connected source) {
    sum += source_value;
    count++;
}
vport_value = sum / count;
```

**Pros**: Prevents clipping
**Cons**: Reduces signal amplitude

### Option 3: Weighted Average
```c
int32_t weighted_sum = 0;
uint16_t total_weight = 0;
for (each connected source) {
    weighted_sum += source_value * source_weight;
    total_weight += source_weight;
}
vport_value = weighted_sum / total_weight;
```

**Pros**: Flexible, user-controllable
**Cons**: More complex, requires weight configuration

### Option 4: Configurable per Port
Each virtual port specifies its mixing algorithm.

**Decision**: Start with averaging for T2/T4 topologies (prevents clipping)

## Data Structures (Revised for Fixed Topologies)

### Topology Instance Configuration
```c
// Configuration for one topology instance
struct topology_instance {
    uint8_t topology_type;      // T1=1, T2=2, T3=3, T4=4 (0=disabled)
    uint8_t accel_inputs[2];    // Which accel axis/axes (0-5 for X,Y,Z,R,P,Y)
    uint8_t func_units[2];      // Which function unit(s) to use (0-7)
    uint8_t midi_outputs[2];    // Which MIDI CC(s) for output (0-127)
    uint8_t enabled;            // Instance active flag
    uint8_t reserved[3];        // Padding for alignment
};
// Total: 8 bytes
```

### Function Unit Configuration
```c
// Function unit with type-specific parameters
struct function_unit {
    uint8_t function_type;      // LINEAR, CURVE, FILTER, etc.
    uint8_t enabled;            // Active flag
    uint8_t param_count;        // Number of valid parameters
    uint8_t reserved;           // Padding
    int16_t params[6];          // Type-specific parameters (12 bytes)
};
// Total: 16 bytes
```

### Virtual Port Runtime State
```c
// Runtime state (not persisted, computed on each sample)
struct virtual_port {
    int16_t value;           // Current value (0-127 MIDI range)
    uint8_t mixer_type;      // SUM, AVG, WEIGHTED_AVG
    uint8_t reserved;        // Padding
};
// Total: 4 bytes
```

### Per-Patch Configuration
```c
struct patch_vport_config {
    struct topology_instance topologies[MAX_TOPOLOGY_INSTANCES];  // 6 × 8 = 48 bytes
    struct function_unit functions[MAX_FUNCTION_UNITS];           // 8 × 16 = 128 bytes
    uint8_t default_mixer_type;                                   // Default mixing algorithm
    uint8_t reserved[7];                                          // Padding to 8-byte boundary
};
// Total: 184 bytes per patch
```

## Processing Flow

### Per-Sample Processing

```
1. Read accelerometer values
2. For each virtual port:
   a. Clear port value
   b. For each input connection to this port:
      - Read source value
      - Apply mixing algorithm
   c. Store final mixed value in port
3. For each function unit:
   a. Read input values from connected virtual ports
   b. Execute function algorithm
   c. Write output value to connected virtual port
4. For each MIDI CC output:
   a. Read value from connected virtual port
   b. Send MIDI CC message if value changed
```

**Note**: May need multiple passes for multi-stage function chains.

### Execution Order Considerations

**Challenge**: Function units may depend on outputs of other function units.

**Solutions**:
1. **Topological Sort**: Determine execution order based on connections
2. **Multi-pass**: Execute all functions multiple times until values stabilize
3. **Explicit Ordering**: User configures execution order
4. **Feedback Delay**: Previous sample values for feedback paths

**Decision**: TBD

## Usage Examples

### Example 1: Complete Processing with Global Calibration

This example shows the recommended approach using the layered architecture:

```c
#include "topology_processor.h"
#include "config_storage.h"

void process_sensor_sample(void)
{
    static struct config_data config;
    static struct topology_processor processor;
    int16_t raw_sensor_values[6];
    int16_t calibrated_values[6];
    
    /* 1. HARDWARE LAYER: Read raw sensor values */
    read_accelerometer(&raw_sensor_values[0], &raw_sensor_values[1], &raw_sensor_values[2]);
    read_gyroscope(&raw_sensor_values[3], &raw_sensor_values[4], &raw_sensor_values[5]);
    
    /* 2. CALIBRATION LAYER: Apply global scale/offset */
    topo_proc_apply_global_calibration(
        raw_sensor_values,
        calibrated_values,
        config.global.accel_scale,
        config.global.accel_offset
    );
    
    /* 3. TOPOLOGY LAYER: Process through virtual ports and functions */
    topo_proc_set_accel_inputs(&processor, calibrated_values);
    topo_proc_execute(&processor);
    
    /* 4. OUTPUT: Send MIDI CC messages */
    for (int i = 0; i < MAX_MIDI_OUTPUTS; i++) {
        uint8_t cc_value = topo_proc_get_midi_output(&processor, i);
        send_midi_cc(16 + i, cc_value);  // CC 16-21
    }
}
```

### Example 2: Configure Global Calibration

```c
/* Set X-axis: ±1g maps to full working range */
config.global.accel_scale[SENSOR_ACCEL_X] = 1000;  // 1000mg = 1g
config.global.accel_offset[SENSOR_ACCEL_X] = 0;    // Neutral at 0g

/* Set Y-axis: ±2g maps to full working range, with 200mg offset */
config.global.accel_scale[SENSOR_ACCEL_Y] = 2000;  // 2000mg = 2g
config.global.accel_offset[SENSOR_ACCEL_Y] = 200;  // Neutral at +0.2g

/* Save to persistent storage */
config_storage_save(&config);
```

### Example 3: Configure a Simple Topology (T1)

```c
/* Configure instance 0: X-axis → Linear function → MIDI CC 16 */
struct topology_instance *topo = &config.patches[0].topologies[0];

topo->topology_type = TOPO_T1;
topo->accel_inputs[0] = SENSOR_ACCEL_X;
topo->func_units[0] = 0;     // Use function unit 0
topo->midi_outputs[0] = 16;  // Output to CC 16
topo->enabled = 1;

/* Configure the function unit for this topology */
struct function_unit *func = &config.patches[0].functions[0];
func_init_linear(func, 
    -127, 127,    // Input range (calibrated sensor values)
    0, 127        // Output range (MIDI)
);
```

### Example 4: Architecture Layer Separation

The three-layer architecture separates concerns:

```
┌─────────────────────────────────────────────┐
│ HARDWARE LAYER                              │
│ - Physical sensors (shared)                 │
│ - Raw readings: X=+1234mg, Y=-567mg, Z=+890mg
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ CALIBRATION LAYER                           │
│ - Global scale/offset (shared)              │
│ - Apply: cal = ((raw - offset) * 127) / scale
│ - Output: X=+62, Y=-36, Z=+45 (int16_t)     │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│ TOPOLOGY LAYER (per-patch)                  │
│ - Virtual ports + function units            │
│ - Per-patch transformations (LINEAR, etc.)  │
│ - Output: MIDI CC values 0-127              │
└─────────────────────────────────────────────┘
```

**Key Benefits**:
1. **Hardware layer**: Multiple patches access same sensor data
2. **Calibration layer**: One calibration applies everywhere (consistency)
3. **Topology layer**: Each patch has unique signal processing

## Configuration Storage

The virtual port configuration must be stored persistently:

### Storage Location
- Add to existing `config_data` structure in [config_storage.h](src/config_storage.h)
- Per-patch configuration (each of 4 patches can have different topology/function configurations)

### Storage Size Estimate

Since the system now uses **4 fixed connection topologies** rather than arbitrary connection tables, storage requirements are significantly reduced.

#### Per-Topology Instance Storage

Each configured topology instance requires:

```c
struct topology_instance {
    uint8_t topology_type;      // T1, T2, T3, or T4 (1 byte)
    uint8_t accel_inputs[2];    // Which accel axis/axes (2 bytes)
    uint8_t func_units[2];      // Which function unit(s) (2 bytes)
    uint8_t midi_outputs[2];    // Which MIDI CC(s) (2 bytes)
    uint8_t enabled;            // Instance active flag (1 byte)
    // Function parameters stored separately in function unit config
};
// Total: 8 bytes per topology instance
```

#### Function Unit Storage

Each function unit stores its parameters based on function type:

```c
struct function_unit {
    uint8_t function_type;      // LINEAR, CURVE, FILTER, etc. (1 byte)
    uint8_t enabled;            // Active flag (1 byte)
    uint8_t params[14];         // Type-specific parameters (14 bytes)
};
// Total: 16 bytes per function unit
```

#### Total Storage Per Patch

Using the data structures defined above:

```
Topology instances:   6 instances × 8 bytes   = 48 bytes
Function units:       8 units × 16 bytes      = 128 bytes
Mixer config:         1 byte + 7 padding      = 8 bytes
------------------------------------------------------------
Total per patch:                                184 bytes
```

Virtual port runtime state (16 ports × 4 bytes = 64 bytes) is **not persisted** - it's computed fresh on each sample.

**For 4 patches**: 184 bytes × 4 = **~736 bytes**

This is **significantly smaller** than the original ~2.5KB estimate for arbitrary connection tables.

#### Comparison to Arbitrary Connection Model

| Storage Model | Per Patch | 4 Patches | Reduction |
|---------------|-----------|------------|-----------|
| Arbitrary Connections | ~640 bytes | ~2.5 KB | - |
| Fixed Topologies | 184 bytes | ~736 bytes | **71% smaller** |

#### Benefits of Fixed Topology Storage

1. **Compact**: Topology type is 1 byte vs. potentially dozens of connection records
2. **Validated**: Only valid topologies can be configured (no invalid routing)
3. **Simple**: Easy to serialize/deserialize for import/export
4. **Fast**: Quick to parse and apply at boot time

## User Interface

### CLI Commands (Revised for Fixed Topologies)

#### Show Topology Configuration
```
topo show               # Show all configured topology instances
topo show 0             # Show specific topology instance
topo list               # List available topology types (T1-T4)
```

Example output:
```
Instance 0: T1 (Simple Linear)
  Accel: X (0)
  Function: 0
  MIDI CC: 16
  Status: Enabled

Instance 1: T2 (Two Inputs Merged)
  Accel: X (0), Y (1)
  Function: 1
  MIDI CC: 17
  Status: Enabled
```

#### Configure Topology Instance
```
topo config 0 type T1                    # Set instance 0 to topology type T1
topo config 0 accel 0                    # Set accel input to X-axis
topo config 0 func 0                     # Use function unit 0
topo config 0 midi 16                    # Output to MIDI CC 16
topo config 0 enable                     # Enable this instance

# For T2/T4 (multiple inputs)
topo config 1 type T2                    # Two inputs merged
topo config 1 accel 0 1                  # X and Y axes
topo config 1 func 1                     # Use function unit 1
topo config 1 midi 17                    # Output to MIDI CC 17

# For T3 (multiple outputs)
topo config 2 type T3                    # One input, two outputs
topo config 2 accel 2                    # Z-axis
topo config 2 func 2                     # Use function unit 2
topo config 2 midi 18 19                 # Output to CC 18 and 19

# For T4 (cascaded)
topo config 3 type T4                    # Cascaded functions
topo config 3 accel 0 1                  # X and Y axes
topo config 3 func 3 4                   # Use function units 3 and 4
topo config 3 midi 20 21                 # Output to CC 20 and 21
```

#### Quick Setup Commands
```
topo quick all_linear                    # Configure all 6 axes as T1 topologies
topo quick default                       # Restore default configuration
```

#### Disable/Clear
```
topo config 0 disable                    # Disable instance 0
topo clear 0                             # Clear/reset instance 0 configuration
topo clear all                           # Clear all instances
```

#### Function Configuration (per function unit)
```
func show 0                              # Show function unit 0 configuration
func config 0 type linear                # Set function type
func config 0 param 0 -2000             # Set parameter 0 (e.g., min_accel)
func config 0 param 1 2000              # Set parameter 1 (e.g., max_accel)
```

### GUI/Visual Editor (Future)
A graphical patch editor would be ideal for topology configuration:
- Select topology type from T1-T4
- Dropdown menus for accel/function/MIDI assignments
- Visual representation of active signal flow
- Real-time value monitoring

## Standard Connection Topologies

The system supports various routing patterns. The following 4 standard topologies cover common use cases and serve as building blocks for more complex configurations.

### Topology 1: Simple Linear Processing Chain

**Description**: Single accelerometer axis processed through one function unit to one MIDI CC output.

**Pattern**: `Accel → VP → Function → VP → MIDI CC`

**Diagram**:
```
┌──────────┐      ┌──────┐      ┌──────────┐      ┌──────┐      ┌──────────┐
│ Accel[X] │─────→│ VP[0]│─────→│ Func[0]  │─────→│ VP[1]│─────→│ MIDI CC  │
└──────────┘      └──────┘      └──────────┘      └──────┘      │  [16]    │
                                                                └──────────┘
```

**Use Case**: Apply filtering, curve shaping, or scaling to a single axis before MIDI output.

**Resource Usage**:
- Virtual Ports: 2
- Function Units: 1
- Accelerometer Sources: 1
- MIDI Outputs: 1

**Configuration Example**:
```c
// Accel-X → VP[0]
{ .source_type = SRC_ACCEL, .source_id = 0, .vport_num = 0 }

// VP[0] → Func[0].input[0]
{ .vport_num = 0, .dest_type = DEST_FUNC, .dest_id = 0, .dest_input = 0 }

// Func[0].output → VP[1]
{ .source_type = SRC_FUNC, .source_id = 0, .vport_num = 1 }

// VP[1] → MIDI CC[16]
{ .vport_num = 1, .dest_type = DEST_MIDI_CC, .dest_id = 16 }
```

---

### Topology 2: Two Inputs Merged

**Description**: Two accelerometer axes merged (mixed) and processed through one function unit to one MIDI CC output.

**Pattern**: `Accel₁ + Accel₂ → VP (mix) → Function → VP → MIDI CC`

**Diagram**:
```
┌──────────┐      
│ Accel[X] │─────┐
└──────────┘     │
                 ├────→┌──────┐      ┌──────────┐      ┌──────┐      ┌──────────┐
┌──────────┐     │     │ VP[0]│─────→│ Func[0]  │─────→│ VP[1]│─────→│ MIDI CC  │
│ Accel[Y] │─────┘     │(mixer)      └──────────┘      └──────┘      │  [16]    │
└──────────┘           └──────┘                                      └──────────┘
```

**Use Case**: Combine multiple motion axes (e.g., X+Y for diagonal movement) before processing. Useful for gesture recognition or composite control.

**Resource Usage**:
- Virtual Ports: 2
- Function Units: 1
- Accelerometer Sources: 2
- MIDI Outputs: 1

**Configuration Example**:
```c
// Accel-X → VP[0], Accel-Y → VP[0] (both write to same port = mixing)
{ .source_type = SRC_ACCEL, .source_id = 0, .vport_num = 0 },
{ .source_type = SRC_ACCEL, .source_id = 1, .vport_num = 0 },

// VP[0] → Func[0].input[0]
{ .vport_num = 0, .dest_type = DEST_FUNC, .dest_id = 0, .dest_input = 0 }

// Func[0].output → VP[1]
{ .source_type = SRC_FUNC, .source_id = 0, .vport_num = 1 }

// VP[1] → MIDI CC[16]
{ .vport_num = 1, .dest_type = DEST_MIDI_CC, .dest_id = 16 }
```

---

### Topology 3: One Input, Multiple Outputs (Fan-out)

**Description**: Single accelerometer axis processed through one function unit, output sent to two different MIDI CC numbers.

**Pattern**: `Accel → VP → Function → VP → (MIDI CC₁, MIDI CC₂)`

**Diagram**:
```
                                                         ┌──────────┐
                                                    ┌───→│ MIDI CC  │
                                                    │    │  [CH X]  │
┌──────────┐      ┌──────┐      ┌──────────┐      ┌─┴───┐└──────────┘
│ Accel[X] │─────→│ VP[0]│─────→│ Func[0]  │─────→│VP[1]│
└──────────┘      └──────┘      └──────────┘      └─┬───┘┌──────────┐
                                                    │    │ MIDI CC  │
                                                    └───→│  [CH Y]  │
                                                         └──────────┘
```

**Use Case**: Send the same processed value to control multiple MIDI parameters. Useful for parallel control (e.g., filter cutoff + brightness).

**Resource Usage**:
- Virtual Ports: 2
- Function Units: 1
- Accelerometer Sources: 1
- MIDI Outputs: 2

**Configuration Example**:
```c
// Accel-X → VP[0]
{ .source_type = SRC_ACCEL, .source_id = 0, .vport_num = 0 }

// VP[0] → Func[0].input[0]
{ .vport_num = 0, .dest_type = DEST_FUNC, .dest_id = 0, .dest_input = 0 }

// Func[0].output → VP[1]
{ .source_type = SRC_FUNC, .source_id = 0, .vport_num = 1 }

// VP[1] → MIDI CC[16] and MIDI CC[17] (fan-out)
{ .vport_num = 1, .dest_type = DEST_MIDI_CC, .dest_id = 16 },
{ .vport_num = 1, .dest_type = DEST_MIDI_CC, .dest_id = 17 }
```

---

### Topology 4: Parallel Processing with Cascaded Function

**Description**: Two accelerometer axes merged, processed through first function unit, then output splits to direct MIDI output AND a second cascaded function unit.

**Pattern**: 
```
Accel₁ + Accel₂ → VP → Function₁ → VP → MIDI CC₁
                                    └─→ MIDI CC₂
```

**Diagram**:
```
┌──────────┐                                                       ┌──────────┐
│ Accel[X] │─────┐                                                 │ MIDI CC  │
└──────────┘     │                                       ┌────────→│  [CH X]  │
                 ├────→┌──────┐      ┌──────────┐      ┌─┴──┐      └──────────┘
┌──────────┐     │     │ VP[0]│─────→│ Func[0]  │─────→│VP[1]      ┌──────────┐
│ Accel[Y] │─────┘     │(mixer)      └──────────┘      └─┬──┘      │ MIDI CC  │
└──────────┘           └──────┘                          └────────→│  [CH Y]  │
                                                                   └──────────┘
                                            
```

**Use Case**: Create derivative signals - the first function generates a primary value (sent to CC1), while the second function processes that value further for a related control (sent to CC2). Example: Primary processing for filter cutoff, secondary processing with different curve for resonance.

**Resource Usage**:
- Virtual Ports: 3
- Function Units: 2
- Accelerometer Sources: 2
- MIDI Outputs: 2

**Configuration Example**:
```c
// Accel-X → VP[0], Accel-Y → VP[0] (mixing)
{ .source_type = SRC_ACCEL, .source_id = 0, .vport_num = 0 },
{ .source_type = SRC_ACCEL, .source_id = 1, .vport_num = 0 },

// VP[0] → Func[0].input[0]
{ .vport_num = 0, .dest_type = DEST_FUNC, .dest_id = 0, .dest_input = 0 }

// Func[0].output → VP[1]
{ .source_type = SRC_FUNC, .source_id = 0, .vport_num = 1 }

// VP[1] → MIDI CC[16] (direct output)
{ .vport_num = 1, .dest_type = DEST_MIDI_CC, .dest_id = 16 },

// VP[1] → Func[1].input[0] (cascaded processing)
{ .vport_num = 1, .dest_type = DEST_FUNC, .dest_id = 1, .dest_input = 0 }

// Func[1].output → VP[2]
{ .source_type = SRC_FUNC, .source_id = 1, .vport_num = 2 }

// VP[2] → MIDI CC[17]
{ .vport_num = 2, .dest_type = DEST_MIDI_CC, .dest_id = 17 }
```

---

### Topology Summary Table

| Topology | Accel In | VPs | Functions | MIDI Out | Key Feature |
|----------|----------|-----|-----------|----------|-------------|
| **T1**   | 1        | 2   | 1         | 1        | Simple linear chain |
| **T2**   | 2        | 2   | 1         | 1        | Input mixing |
| **T3**   | 1        | 2   | 1         | 2        | Output fan-out |
| **T4**   | 2        | 3   | 2         | 2        | Cascaded processing |

### Combining Topologies

These topologies can be instantiated multiple times in parallel within the same configuration. For example:

**Six Independent Axes** (6× Topology 1):
```
Accel[X] → VP[0] → Func[0] → VP[6]  → MIDI CC[16]
Accel[Y] → VP[1] → Func[1] → VP[7]  → MIDI CC[17]
Accel[Z] → VP[2] → Func[2] → VP[8]  → MIDI CC[18]
Roll     → VP[3] → Func[3] → VP[9]  → MIDI CC[19]
Pitch    → VP[4] → Func[4] → VP[10] → MIDI CC[20]
Yaw      → VP[5] → Func[5] → VP[11] → MIDI CC[21]
```

This uses 6 function units and 12 virtual ports, fitting within the proposed resource limits (8 function units, 16 virtual ports).

---

## Function Abstraction Units (Placeholder)

Function types to be defined in future documentation:

### Potential Function Types
- **Pass-through**: Direct copy (for routing)
- **Linear scaling**: Map input range to output range
- **Non-linear curves**: Exponential, logarithmic, S-curve
- **Filtering**: Low-pass, high-pass, deadzone
- **Math operations**: Add, subtract, multiply, divide, min, max
- **Logic**: Gates, switches, comparators
- **Envelope**: Attack/decay/sustain/release
- **LFO**: Oscillator for modulation
- **Quantize**: Snap to discrete values
- **Threshold**: Toggle based on input level

**Note**: Each function type will need separate specification document.

## Migration Path

### Phase 1: Core Infrastructure
- Implement virtual port data structures
- Implement fixed topology selection and routing
- Implement basic mixer algorithms
- Add configuration storage with topology_instance structures

### Phase 2: Simple Functions
- Pass-through function
- Linear scaling function  
- Basic CLI for topology configuration

### Phase 3: Processing Engine
- Per-sample processing loop for each topology type
- Fixed execution order based on topology (no topological sort needed)
- Integration with existing accelerometer and MIDI code

### Phase 4: Advanced Functions
- Additional function types as needed
- Complex routing scenarios
- Performance optimization

### Phase 5: User Experience
- Enhanced CLI commands
- Configuration import/export
- Preset management
- Visual editor (if applicable)

## Compatibility Considerations

### Backward Compatibility
- Existing simple linear mapping should continue to work
- Default configuration should provide equivalent behavior
- Migration tool for old configuration format?

### Fallback Mode
If virtual port configuration is corrupted or invalid:
- Revert to simple direct accelerometer → MIDI CC mapping
- Log warning and continue operation

## Open Questions

### Resolved by Fixed Topology Approach

~~1. **Resource limits**: 16 VPs, 8 function units, 6 topology instances per patch~~ ✓  
~~3. **Execution order**: Fixed by topology type~~ ✓  
~~6. **Storage scope**: Per-patch configuration~~ ✓  
~~8. **Configuration validation**: Only valid topologies can be configured~~ ✓

### Still Open

1. **Mixer algorithm**: Which default mixing algorithm? Configurable per port?
   - Averaging seems most practical for T2/T4 topologies
2. **Feedback loops**: Not supported in T1-T4 topologies (no cycles)
3. **Port naming**: Numbered only, or user-assignable names?
   - Numbered approach seems simplest for fixed topologies
4. **Performance**: Processing latency target? CPU usage limits?
   - Need to benchmark per-sample processing of 6 simultaneous topologies
5. **Presets**: Ship with example configurations?
   - Factory defaults should configure all 6 axes using T1 topology
6. **Testing**: How to unit test each topology type?
   - Unit tests for each of T1-T4 topologies
7. **Topology expansion**: How to add T5, T6, etc. in future?
   - Reserve topology type values 5-15 for future expansion

## References

- Current implementation: [MAPPING.md](MAPPING.md)
- Configuration storage: [CONFIG_STORAGE.md](CONFIG_STORAGE.md)
- User interface: [UI_INTERFACE.md](UI_INTERFACE.md)
- Architecture: [ARCHITECTURE.md](ARCHITECTURE.md)

## Revision History

|    Date    | Version | Author  | Changes |
|------------|---------|---------|---------|
| 2026-04-11 |   0.3   | Initial | Revised for fixed topologies: storage (71% reduction), data structures, CLI commands |
| 2026-04-11 |   0.2   | Initial | Added 4 standard connection topologies (T1-T4) |
| 2026-04-11 |   0.1   | Initial | Initial architectural design |


# Virtual Ports GUI Design Specification

**Version:** 1.0  
**Date:** April 11, 2026  
**Platform:** macOS GUI Application  
**Purpose:** Configuration interface for accelerometer/gyroscope to MIDI CC mapping using virtual ports

---

## Overview

This GUI provides visual configuration of the signal routing system that maps 6 motion sensors (3 accelerometer axes + 3 gyroscope axes) through configurable topologies and function units to MIDI CC outputs.

**Key Concepts:**
- **4 Patches**: Switchable configurations (like instrument presets)
- **6 Sensor Sources per Patch**: X, Y, Z accelerometer + Roll, Pitch, Yaw gyroscope
- **4 Topology Types**: Fixed signal routing patterns (T1-T4)
- **8 Function Units**: Signal processing blocks per patch
- **6 MIDI Outputs**: Configurable CC numbers (typically CC 16-21)

---

## Main Window Layout

```
┌─────────────────────────────────────────────────────────────┐
│ GuitarAcc Virtual Ports Configuration                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Patch: [Dropdown: 1-4]  [Load] [Save] [Export] [Import]  │
│                                                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Sensor Source Selection                              │  │
│  │  ┌──┐ ┌──┐ ┌──┐ ┌────┐ ┌──────┐ ┌────┐               │  │
│  │  │ X│ │ Y│ │ Z│ │Roll│ │Pitch │ │Yaw │               │  │
│  │  └──┘ └──┘ └──┘ └────┘ └──────┘ └────┘               │  │
│  │   ^                                                    │  │
│  │  Selected                                              │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Configuration Panel: X-Axis                          │  │
│  │                                                        │  │
│  │  ☑ Enabled                                            │  │
│  │                                                        │  │
│  │  Topology: ◉ T1: Simple Linear                        │  │
│  │            ○ T2: Merged Inputs                         │  │
│  │            ○ T3: Fan-out (Multi-CC)                    │  │
│  │            ○ T4: Cascaded Processing                   │  │
│  │                                                        │  │
│  │  [Topology Diagram - Visual Flow]                     │  │
│  │                                                        │  │
│  │  Function Unit Configuration:                         │  │
│  │    Type: [Dropdown: LINEAR]                           │  │
│  │    Input Range: [-2000] to [+2000] mg                 │  │
│  │    Output Range: [0] to [127] (MIDI)                  │  │
│  │    ☐ Invert Range                                     │  │
│  │                                                        │  │
│  │  MIDI Output:                                          │  │
│  │    CC Number: [16] (16-127)                           │  │
│  │                                                        │  │
│  │  [Test] [Reset to Default] [Copy to Other Axes]       │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │  Live Monitor                                          │  │
│  │  X: -1234 mg → [████████░░] → CC 16: 45              │  │
│  │  Y:   +56 mg → [█████░░░░░] → CC 17: 63              │  │
│  │  Z: +1890 mg → [█████████░] → CC 18: 112             │  │
│  └───────────────────────────────────────────────────────┘  │
│                                                              │
│  [Send to Device] [Load from Device]          [Close]       │
└─────────────────────────────────────────────────────────────┘
```

---

## UI Components Detail

### 1. Patch Selector
**Location:** Top of window  
**Controls:**
- **Dropdown:** Select patch 1-4
- **Load Button:** Load patch from device
- **Save Button:** Save patch to device
- **Export Button:** Export patch to JSON file
- **Import Button:** Import patch from JSON file

**Behavior:**
- Changing patch loads all 6 sensor configurations
- Unsaved changes prompt confirmation dialog

---

### 2. Sensor Source Tabs/Buttons
**Location:** Below patch selector  
**Controls:** 6 buttons representing:
- **X-Axis** (Accelerometer X)
- **Y-Axis** (Accelerometer Y)
- **Z-Axis** (Accelerometer Z)
- **Roll** (Gyroscope X)
- **Pitch** (Gyroscope Y)
- **Yaw** (Gyroscope Z)

**Visual State:**
- **Active:** Button highlighted, shows current selection
- **Enabled:** Green indicator if sensor is configured
- **Disabled:** Gray indicator if sensor is not enabled

**Interaction:**
- Click to switch configuration panel to that sensor
- Right-click for quick actions (disable, copy settings, reset)

---

### 3. Configuration Panel (Per Sensor)

#### 3A. Enable Checkbox
```
☑ Enabled
```
- Toggles whether this sensor axis generates MIDI output
- When unchecked, all controls below are grayed out

#### 3B. Topology Selection
Radio button group with 4 options:

**◉ T1: Simple Linear**
```
Accel → Function → MIDI CC
```
- Single sensor input
- One function unit
- One MIDI CC output
- **Use case:** Direct X-axis tilt → CC 16 for filter cutoff

**○ T2: Merged Inputs**
```
Accel₁ ┐
       ├→ Mixer → Function → MIDI CC
Accel₂ ┘
```
- Two sensor inputs (requires selecting second source)
- Mixer type selector (Average, Sum, Max, Min)
- One function unit
- One MIDI CC output
- **Use case:** Combine X + Y axes → CC 16 for combined movement

**○ T3: Fan-out (Multi-CC)**
```
Accel → Function ┬→ MIDI CC₁
                 └→ MIDI CC₂
```
- Single sensor input
- One function unit
- Two MIDI CC outputs (requires two CC number selections)
- **Use case:** X-axis → CC 16 (filter) AND CC 17 (volume)

**○ T4: Cascaded Processing**
```
Accel₁ ┐
       ├→ Mixer → Function₁ → MIDI CC₁
Accel₂ ┘       └→ Function₂ → MIDI CC₂
```
- Two sensor inputs
- Mixer type selector
- Two function units (requires two function configurations)
- Two MIDI CC outputs
- **Use case:** X+Y average → linear map to CC 16, scaled copy to CC 17

#### 3C. Topology-Specific Controls

**For T2 and T4 (Multiple Inputs):**
```
┌─────────────────────────────────┐
│ Input Sources                    │
│  Primary:   [X-Axis ▼]          │
│  Secondary: [Y-Axis ▼]          │
│                                  │
│ Mixer Type: [Average ▼]         │
│  • Passthrough (first input)    │
│  • Sum (clamp to 0-127)         │
│  • Average (mean of inputs)     │
│  • Maximum                       │
│  • Minimum                       │
└─────────────────────────────────┘
```

**For T3 and T4 (Multiple Outputs):**
```
┌─────────────────────────────────┐
│ MIDI Outputs                     │
│  Output 1: CC [16]              │
│  Output 2: CC [17]              │
└─────────────────────────────────┘
```

#### 3D. Visual Topology Diagram
Dynamic SVG/Canvas rendering showing:
- Sensor input(s) as circles
- Mixer (if applicable) as triangle
- Function unit(s) as rectangles
- Virtual ports as connection points
- MIDI outputs as MIDI plug icons
- Signal flow arrows

**Example for T1:**
```
┌────────┐    ┌──────────┐    ┌──────────┐
│ X-Axis │───→│ LINEAR   │───→│ CC 16    │
│ -2000mg│    │ -2000→0  │    │ Value:45 │
└────────┘    │ +2000→127│    └──────────┘
              └──────────┘
```

#### 3E. Function Unit Configuration

**Function Type Selector:**
```
Type: [LINEAR            ▼]
      PASSTHROUGH
      LINEAR
      DEADZONE
      INVERT
      SCALE
      CLAMP
```

**Type-Specific Parameters:**

**LINEAR:**
```
┌────────────────────────────────────┐
│ Input Range (mg or deg/s):         │
│  Min: [-2000] Max: [+2000]         │
│                                     │
│ Output Range (MIDI):                │
│  Min: [0] Max: [127]                │
│                                     │
│ ☐ Invert (swap input min/max)      │
│                                     │
│ Preview:                            │
│  Input: 0 → Output: 63              │
│  [══════╬══════]                    │
│        center                        │
└────────────────────────────────────┘
```

**DEADZONE:**
```
┌────────────────────────────────────┐
│ Threshold: [100] mg                 │
│                                     │
│ Behavior:                           │
│  • Values below threshold → 0      │
│  • Values above threshold → pass   │
│                                     │
│ Preview:                            │
│  [-100 to +100] → 0                │
│  [+100 to +2000] → original value  │
└────────────────────────────────────┘
```

**SCALE:**
```
┌────────────────────────────────────┐
│ Scale Factor: [1.5] (0.1 - 3.0)    │
│                                     │
│ ☑ Clamp to MIDI range (0-127)      │
│                                     │
│ Preview:                            │
│  Input: 50 → Output: 75             │
└────────────────────────────────────┘
```

**CLAMP:**
```
┌────────────────────────────────────┐
│ Output Limits:                      │
│  Min: [20] Max: [100]              │
│                                     │
│ Constrains MIDI output to range    │
└────────────────────────────────────┘
```

**INVERT:**
```
┌────────────────────────────────────┐
│ Inverts MIDI value:                 │
│  0 ↔ 127                            │
│  63 ↔ 64                            │
│                                     │
│ (No parameters required)            │
└────────────────────────────────────┘
```

**PASSTHROUGH:**
```
┌────────────────────────────────────┐
│ Direct pass-through                 │
│ (No parameters required)            │
└────────────────────────────────────┘
```

#### 3F. MIDI Output Assignment
```
┌────────────────────────────────────┐
│ MIDI CC Number: [16]               │
│                                     │
│ Common assignments:                 │
│  1  - Modulation                    │
│  7  - Volume                        │
│  10 - Pan                           │
│  11 - Expression                    │
│  16-31 - General Purpose (typical)  │
│  71-74 - Filter, Resonance, etc.   │
└────────────────────────────────────┘
```

#### 3G. Action Buttons
- **Test:** Send test values through pipeline, show in Live Monitor
- **Reset to Default:** Restore T1 with linear ±2g mapping
- **Copy to Other Axes:** Dialog to select which axes receive this configuration

---

### 4. Live Monitor Panel
Shows real-time sensor → MIDI conversion for all enabled axes:

```
┌──────────────────────────────────────────────────────────┐
│ Live Monitor (Updated at 20 Hz)                          │
├──────────────────────────────────────────────────────────┤
│ X: -1234 mg → [████████░░░░░░] → CC 16: 45             │
│ Y:   +56 mg → [███████░░░░░░░] → CC 17: 63             │
│ Z:  +1890 mg → [█████████████░] → CC 18: 112           │
│ Roll:  0 °/s → [░░░░░░░░░░░░░░] → CC 19: 0  (disabled) │
│ Pitch: 0 °/s → [░░░░░░░░░░░░░░] → CC 20: 0  (disabled) │
│ Yaw:   0 °/s → [░░░░░░░░░░░░░░] → CC 21: 0  (disabled) │
└──────────────────────────────────────────────────────────┘
```

Features:
- Raw sensor value (left)
- Visual bar graph of MIDI value
- Final MIDI CC number and value (right)
- Grayed out if disabled
- Color coding:
  - Green: Normal operation
  - Yellow: Near limits (0-10, 117-127)
  - Red: At limits (0, 127)

---

## Configuration Workflow Examples

### Example 1: Basic Tilt Control
**Goal:** Map X-axis tilt to MIDI CC 16 for filter cutoff

1. Select Patch 1
2. Click **X-Axis** button
3. Check **☑ Enabled**
4. Select **◉ T1: Simple Linear**
5. Function Type: **LINEAR**
6. Input Range: `-2000` to `+2000` mg
7. Output Range: `0` to `127`
8. MIDI CC: `16`
9. Click **Send to Device**

Result: Tilting guitar left/right sweeps filter 0-127

---

### Example 2: Combined Movement
**Goal:** Average X and Y axes, send to one CC

1. Select **X-Axis** button
2. Check **☑ Enabled**
3. Select **◉ T2: Merged Inputs**
4. Primary: **X-Axis**, Secondary: **Y-Axis**
5. Mixer: **Average**
6. Function Type: **LINEAR**
7. Input Range: `-2000` to `+2000` mg
8. Output Range: `0` to `127`
9. MIDI CC: `16`
10. Click **Send to Device**

Result: Combined X+Y movement controls CC 16

---

### Example 3: Multi-Destination
**Goal:** One sensor controls two different CCs

1. Select **Z-Axis** button
2. Check **☑ Enabled**
3. Select **◉ T3: Fan-out**
4. Function Type: **LINEAR**
5. Input Range: `-2000` to `+2000` mg
6. Output Range: `0` to `127`
7. Output 1 CC: `16` (Filter)
8. Output 2 CC: `7` (Volume)
9. Click **Send to Device**

Result: Z-axis tilt controls both filter and volume simultaneously

---

### Example 4: Advanced Processing
**Goal:** X+Y average → linear to CC 16, scaled version to CC 17

1. Select **X-Axis** button
2. Check **☑ Enabled**
3. Select **◉ T4: Cascaded Processing**
4. Primary: **X-Axis**, Secondary: **Y-Axis**
5. Mixer: **Average**
6. Function 1:
   - Type: **LINEAR**
   - Range: `-2000` to `+2000` → `0` to `127`
   - Output: CC `16`
7. Function 2:
   - Type: **SCALE**
   - Factor: `1.5`
   - Output: CC `17`
8. Click **Send to Device**

Result: Combined movement → CC 16 (linear), CC 17 (enhanced 1.5x)

---

## Data Model (JSON Export Format)

```json
{
  "patch_number": 1,
  "patch_name": "Basic Tilt Controls",
  "default_mixer_type": "AVERAGE",
  "axes": [
    {
      "source": "X_AXIS",
      "enabled": true,
      "topology_type": "T1",
      "accel_inputs": [0],
      "function_units": [
        {
          "index": 0,
          "type": "LINEAR",
          "in_min": -2000,
          "in_max": 2000,
          "out_min": 0,
          "out_max": 127
        }
      ],
      "midi_outputs": [16]
    },
    {
      "source": "Y_AXIS",
      "enabled": true,
      "topology_type": "T1",
      "accel_inputs": [1],
      "function_units": [
        {
          "index": 1,
          "type": "LINEAR",
          "in_min": -2000,
          "in_max": 2000,
          "out_min": 0,
          "out_max": 127
        }
      ],
      "midi_outputs": [17]
    },
    {
      "source": "Z_AXIS",
      "enabled": false,
      "topology_type": "DISABLED"
    },
    {
      "source": "ROLL",
      "enabled": false,
      "topology_type": "DISABLED"
    },
    {
      "source": "PITCH",
      "enabled": false,
      "topology_type": "DISABLED"
    },
    {
      "source": "YAW",
      "enabled": false,
      "topology_type": "DISABLED"
    }
  ]
}
```

---

## Communication Protocol

### Device → GUI (Status Query)
```
Command: "topo show <patch_num>"
Response: Binary structure or JSON
```

### GUI → Device (Configuration)
```
Command: "topo config <axis> <topology> <params>"
Example: "topo config 0 T1 16 0 127"
         (Axis 0, T1, CC 16, func 0, MIDI 0-127)
```

### Live Monitoring
```
Subscribe to UART output stream:
"accel: X=-1234 Y=56 Z=1890"
"midi: CC16=45 CC17=63 CC18=112"
```

---

## Visual Design Recommendations

### Color Scheme
- **Primary:** Blue (#2196F3) for selected elements
- **Success:** Green (#4CAF50) for enabled/active
- **Warning:** Yellow (#FFC107) for near-limits
- **Error:** Red (#F44336) for limits/errors
- **Neutral:** Gray (#9E9E9E) for disabled

### Typography
- **Headers:** 16pt Bold
- **Labels:** 12pt Regular
- **Values:** 14pt Monospace (for sensor readings)

### Spacing
- 16px padding around panels
- 8px spacing between controls
- 24px spacing between major sections

---

## Accessibility

1. **Keyboard Navigation:**
   - Tab through all controls
   - Arrow keys for radio buttons
   - Enter/Space for buttons

2. **Screen Reader Support:**
   - All controls properly labeled
   - Dynamic values announced on change
   - Topology diagrams have text descriptions

3. **High Contrast Mode:**
   - Alternative color scheme available
   - Minimum 4.5:1 contrast ratio

---

## Implementation Notes for AI Assistant

### Suggested macOS Technologies
- **SwiftUI** for modern declarative UI
- **Combine** for reactive data bindings
- **ORSSerialPort** for USB/UART communication
- **CorePlot** or **Charts** framework for live graphs

### Key SwiftUI Views
```swift
struct PatchConfigView: View
struct SensorSelectorView: View
struct TopologyConfigPanel: View
struct FunctionUnitEditor: View
struct LiveMonitorView: View
struct TopologyDiagram: View
```

### State Management
```swift
class VirtualPortsConfig: ObservableObject {
    @Published var currentPatch: Int
    @Published var axes: [AxisConfig]
    @Published var liveValues: [SensorReading]
}
```

### Serial Communication
- Baud rate: 115200
- Line endings: \r\n
- Command/response pattern with timeouts
- Binary protocol option for efficiency

---

## Future Enhancements

1. **Visual Programming:** Drag-and-drop topology creation
2. **Calibration Wizard:** Interactive sensor range detection
3. **MIDI Learn:** Click CC field, move controller to assign
4. **Presets Library:** Community-shared configurations
5. **Performance Mode:** Simplified view for live use
6. **Multi-Device:** Manage multiple guitar units
7. **Patch Bank Management:** Organize 4 patches into banks
8. **Undo/Redo:** Configuration change history

---

## Testing Scenarios

1. **Load default patch** → Verify all 6 axes show T1 with CC 16-21
2. **Change topology** → Dialog updates with appropriate controls
3. **Enable/disable axis** → Live monitor reflects state
4. **Send to device** → Verify UART commands transmitted
5. **Load from device** → Parse response and populate UI
6. **Export/Import JSON** → Round-trip data integrity
7. **Live monitoring** → 20 Hz update with smooth animation
8. **Error handling** → Invalid CC numbers rejected, device disconnection detected

---

## Reference Implementation Checklist

- [ ] Main window layout with patch selector
- [ ] 6 sensor source buttons with state indicators
- [ ] Topology type radio button group
- [ ] Dynamic configuration panel based on topology
- [ ] Function unit editor with all 6 types
- [ ] Visual topology diagram renderer
- [ ] Live monitor with bar graphs and color coding
- [ ] Serial port connection manager
- [ ] Command protocol implementation
- [ ] JSON export/import functionality
- [ ] Error handling and validation
- [ ] Keyboard shortcuts and accessibility
- [ ] Unit tests for data model
- [ ] Integration tests with mock device

---

**End of UI Specification**

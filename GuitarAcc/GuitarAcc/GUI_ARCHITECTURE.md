// GuitarAcc Basestation GUI Architecture (macOS)
//
// Overview
// This document describes the architecture for a macOS application (GUI) that interfaces with the GuitarAcc Basestation via its Zephyr Shell CLI over a USB serial (VCOM) connection. The GUI acts as a command center for configuring, monitoring, and managing the basestation. It does not process MIDI data directly; all MIDI communication is handled by the basestation firmware.
//
// Key Responsibilities
// - Discover and connect to the basestation over a USB serial port.
// - Send CLI commands (e.g., set MIDI channel, select MIDI patch, export/import configurations).
// - Parse and present CLI command responses in a user-friendly way.
// - Provide a graphical interface for:
//     - Viewing status and configuration
//     - Changing MIDI and system settings
//     - Exporting/importing configuration
//     - Monitoring runtime state
//
// System Context
// - Basestation: Embedded device running Zephyr Shell on USB VCOM (115200 baud, 8N1, no flow control).
// - GUI (macOS App): Swift/SwiftUI application communicating as a serial terminal client.
// - Clients: BLE guitar devices (not directly managed by the GUI).
//
// Communication Model
// - The GUI opens the correct /dev/tty.usbmodem* serial port.
// - Sends CLI commands as plain text (terminated by \n or \r\n).
// - Reads line-oriented text responses from the basestation.
// - Some commands (e.g., config export) may return multi-line or JSON output.
// - The GUI must maintain stateful interaction, handling prompts, streaming logs, command history, and multi-line input sessions (e.g., for imports).
//
// macOS Implementation Notes
// - Serial Port Discovery and Selection:
//     - On launch or when connecting, the GUI discovers all serial ports matching the pattern `/dev/tty.usbmodem*`.
//     - There are typically two such ports; only the "application interface" should be used for CLI communication (the other is for network connection).
//     - The GUI attempts to connect to the lowest-numbered port first, opening it with the configuration: 115200 baud, timeout=1, rtscts=True.
//     - It sends a known CLI command (such as `status` or `help`) and checks for a valid response.
//     - If a valid CLI response is received, this port is used as the CLI interface.
//     - If not, the GUI disconnects and attempts to connect to the second port, repeating the probe.
//     - The port string will look like `/dev/tty.usbmodem0010501494421` (exact suffix may vary by device).
//     - All connection attempts should handle errors gracefully and notify the user if no valid CLI port is found.
// - Serial Communication: Use ORSSerialPort or equivalent for Swift serial port access, with support for line-based reading and timeouts.
// - UI Technologies: Prefer SwiftUI for all user-facing components; use AppKit bridges only if needed.
// - Concurrency: Leverage Swift Concurrency (async/await) for serial I/O and UI updates.
// - Response Parsing: Implement parsers for both plain CLI output and JSON-formatted responses (for configuration import/export).
// - Device Discovery: Poll /dev/tty.usbmodem* to auto-detect the basestation, allow user selection if multiple devices.
// - State Management: Cache last known device state (config, status) for user feedback.
//
// GUI Functional Areas
// - Connection Management:
//     - Show list of available VCOM ports
//     - Allow connect/disconnect
// - Status Monitoring:
//     - Periodically issue status and midi rx_stats commands
//     - Display connected devices, MIDI state, and runtime stats
// - Configuration Management:
//     - View current config (config show or parsed export)
//     - Change MIDI channel, patch, CC mappings (send config midi_ch <n>, config select <n>, etc.)
//     - Export/import configuration as JSON (with validation and error reporting)
//     - Restore defaults, save changes
// - Command Execution:
//     - Map GUI actions to CLI commands, display output/errors
//     - Provide advanced/terminal view for direct CLI access (optional)
//
// Example Workflow
// 1. User launches GUI
// 2. GUI discovers basestation serial port, user connects
// 3. Status and configuration fetched and shown
// 4. User sets new MIDI channel (GUI issues config midi_ch 2)
// 5. User exports configuration (GUI issues config export, parses JSON)
// 6. User imports configuration (GUI issues config import, streams JSON line-by-line)
// 7. User monitors status and MIDI statistics in real time
//
// Error Handling
// - Notify user of connection failures, timeouts, or unexpected CLI responses
// - Validate responses for success/failure and show errors clearly
// - Handle multi-line command sessions gracefully (e.g., config import)
//
// Extensibility
// - New CLI commands are supported by updating the GUI's mapping and parsers
// - Underlying protocol is text/JSON over serial; robust to CLI enhancements
//
// Security/Permissions
// - macOS app requires user permission to access serial ports
// - Recommend notarization and appropriate entitlements for distribution
//
// References
// - UI_INTERFACE.md — https://github.com/richmaes/guitaracc/blob/rotation_port/basestation/UI_INTERFACE.md - Use the 'rotation_port' branch
// - ARCHITECTURE.md — Overall system architecture of the hardware device - https://github.com/richmaes/guitaracc/blob/rotation_port/ARCHITECTURE.md - Use the 'rotation_port' branch
//
// This architecture provides a modern, robust way to manage and configure the GuitarAcc Basestation from macOS, leveraging the existing CLI for all device communication and logic.
//
// GUI Views
//
// Global Settings View:
// - Purpose:
//     Display and allow editing of global configuration values such as MIDI channel, BLE scan interval, LED brightness, running average settings, and other global parameters.
// - Data Handling:
//     Values are fetched from the basestation using CLI commands and updates are performed by issuing the corresponding CLI commands (e.g., `config midi_ch <n>`).
// - User Interaction:
//     Users can edit fields representing the global settings and apply/save changes via a dedicated button.
//     Validation is performed on input fields, with clear feedback and error reporting in case of invalid values or command failures.
//
// Patch Configuration View:
// - Purpose:
//     Display and allow editing of per-patch configuration settings including patch name, velocity curve, CC mapping, accel minimum/maximum/invert settings, and other patch-specific parameters.
// - Data Handling:
//     This view is reused for all 16 patches. Patch data is fetched and saved using CLI commands such as `config patch <n>`, `config select <n>`, etc.
// - User Interaction:
//     Users select the active patch using a UI control such as a tab bar, dropdown, or segmented control at the top of the view.
//     Edits affect only the currently selected patch and require explicit user confirmation (e.g., via a save/apply button) to persist changes.
//
// Patch Selection & Sync Behavior
// - Overview:
//     The Patch view stays synchronized with the basestation’s currently active patch and updates the device when the user selects a different patch.
// - On View Appear:
//     1. Query the device for the currently selected patch (tries `config show`, then `status`).
//     2. Set the UI’s selected patch to match the device.
//     3. Export the selected patch configuration using `config export patch <n>` and display it in the view (raw text/JSON until structured parsing is implemented).
// - On Patch Change (user taps a different patch):
//     1. Issue `config select <n>` to switch the device’s active patch.
//     2. Issue `config export patch <n>` to retrieve the new patch configuration.
//     3. Update the view with the exported configuration.
// - Error Handling:
//     - If the device is disconnected, the UI disables patch actions until reconnected.
//     - If a command times out or returns an error, the view shows the last known export and a log entry appears in the CLI panel.
// - Concurrency/UX:
//     - The view prevents overlapping operations (debounce/cancellation as needed) to keep the UI responsive when switching patches quickly.
//     - Auto-reconnect attempts continue in the background; when connection is re-established, the view can re-run the export for the currently selected patch.
// - Connection-State Sync:
//     - If the Patch view is visible and the device connection becomes established (after auto-reconnect), the view re-queries the current patch index and re-exports the configuration to ensure the UI reflects the device state.
//
// - No Default Patch Assumption:
//     - The GUI does not blindly assume patch 0 on startup. It queries the device for the current patch index; only if the device does not report a value does the GUI retain the current UI selection.
//
// - Logging / Diagnostics:
//     - During initial sync, the GUI logs: "Sync: querying current patch index…" and then the discovered index and source (e.g., from `config show` or `status`).
//     - If the index cannot be determined, the GUI logs: "Sync: could not determine current patch index." and proceeds using the current UI selection.
//
// Device CLI Contract (Patch)
// - Commands:
//     - `config select <n>`: Makes patch `<n>` the active patch (0–15).
//     - `config export patch <n>`: Outputs the full configuration for patch `<n>` (text/JSON; treated as multi-line output).
//     - `config show` / `status`: Used to infer the currently selected patch when the view appears.
// - Responses:
//     - `config select <n>` should complete quickly; any errors should be surfaced in the CLI output.
//     - `config export patch <n>` may return multi-line output (often JSON); the GUI collects until a quiet period.
// - Notes:
//     - The GUI currently displays the raw export text. A future enhancement will parse this into a structured model for field-by-field editing and saving.
//
// Example: Patch Export Output (firmware v2, captured from real device)
// - The device returns JSON for `config export patch <n>`. Topology and function unit routing
//   data are embedded in the patch JSON (firmware v2+). ANSI escape sequences and the trailing
//   prompt must be stripped before JSON decoding.
//
// ```json
// {
//   "version": 1,
//   "config": {
//     "patches": [
//       {
//         "patch_num": 0,
//         "patch_name": "Patch 0",
//         "led_mode": 0,
//         "midi_deadzone": 1,
//         "default_mixer_type": 2,
//         "topologies": [
//           { "instance": 0, "enabled": true, "topology_type": 1, "accel_inputs": [0,0], "func_units": [0,0], "midi_outputs": [16,0] },
//           ...
//           { "instance": 5, "enabled": true, "topology_type": 1, "accel_inputs": [5,0], "func_units": [5,0], "midi_outputs": [21,0] }
//         ],
//         "functions": [
//           { "unit": 0, "enabled": true, "function_type": 2, "param_count": 4, "params": [-2000,2000,0,127,0,0] },
//           ...
//           { "unit": 7, "enabled": true, "function_type": 2, "param_count": 4, "params": [-2000,2000,0,127,0,0] }
//         ]
//       }
//     ]
//   }
// }
// ```
//
// Notes on v2 format:
// - cc_mapping, velocity_curve, accel_min/max/invert absent; routing expressed via topologies array.
// - 6 topology instances (0-5) map accelerometer axes to MIDI CCs via function units.
// - 8 function units (0-7) define LINEAR mappings (in_min, in_max, out_min, out_max).
// - Trailing prompt ESC[1;32mGuitarAcc:~$ ESC[m stripped by CLIOutputParser.stripANSI.
//
// Parsing approach (implemented in CLIOutputParser):
// - extractJSON(from:): extracts JSON object via brace balancing after ANSI strip.
// - parsePatchExport(from:): decodes into PatchConfig using convertFromSnakeCase; optional fields for older firmware.
// - virtualPortConfigs(from:): converts topologies + functions into [VirtualPortConfig] for UI binding.
//
// Save/Sync Behavior:
// - Purpose:
//     Provide an explicit action to synchronize the GUI with the currently selected patch on the basestation.
// - Behavior on Press:
//     1. Issue `config select <n>` to instruct the basestation to load/activate the selected patch (where `<n>` is the patch index chosen in the UI).
//     2. Issue `config export patch <n>` to retrieve the full configuration for that patch (typically JSON/multi-line text).
//     3. Parse and display the resulting configuration in the Patch Configuration View so users can review the live settings.
// - Notes:
//     - The GUI may also automatically perform step (2) when the user changes the selected patch, but the explicit Save/Sync button ensures users can refresh on demand.
//     - If the export returns JSON, the GUI should attempt to parse it into a structured model for field-by-field editing; until parsing is implemented, the raw JSON/text can be displayed.
//     - Error cases (timeouts, invalid responses) should be surfaced to the user with clear feedback, leaving the last known configuration visible.
//
// Data Flow:
// - User presses Save/Sync → GUI sends `config select <n>` → GUI sends `config export patch <n>` → GUI collects multi-line output → GUI parses (or displays raw) → Patch view updates.
//
// Implementation Status:
// - [x] Save/Sync button added to Patch view that triggers `config select` and `config export patch`.
// - [x] USBSerialManager helper to execute a command and collect multi-line output with per-line timeout.
// - [x] Parse exported patch config JSON into PatchConfig model (CLIOutputParser.parsePatchExport); v2 firmware includes topologies and functions arrays.
// - [x] Derive VirtualPortConfig instances from PatchConfig via CLIOutputParser.virtualPortConfigs(from:); bound to VirtualPortControlsSection on discovery.
// - [ ] Persist edits back to device via appropriate `config` commands.
//
// Status & Monitoring View:
// - Purpose:
//     Display real-time system status including number of connected devices, MIDI output state, and configuration area status.
// - Data Handling:
//     Data is fetched with the `status` command and periodically refreshed.
// - User Interaction:
//     Read-only view displaying status fields; may provide a manual refresh button.
//
// MIDI Statistics & Diagnostics View:
// - Purpose:
//     Show live MIDI receive statistics such as total bytes, clock messages, BPM, and message counts.
// - Data Handling:
//     Uses `midi rx_stats` command for data and may offer a reset button (`midi rx_reset`).
// - User Interaction:
//     Real-time or on-demand updates; reset statistics option.
//
// Configuration Import/Export View:
// - Purpose:
//     Manage backup/restore operations for global and per-patch config using JSON import/export.
// - Data Handling:
//     Uses CLI commands (`config export`, `config import`, `config export global`, `config export patch <n>`). Handles multi-line input and output. Provides feedback and error reporting.
// - User Interaction:
//     File chooser, text import/export, validation feedback, progress indication.
//
// Advanced/Terminal View:
// - Purpose:
//     Let experienced users send arbitrary CLI commands or interact with the shell directly. Show live log output as needed.
// - Data Handling:
//     Raw text I/O to/from the CLI. Provides full transparency and flexibility.
// - User Interaction:
//     Command entry field with history, output area, scrollback, and possibly logging toggle.
//
// CLI Interaction Display:
// - Purpose:
//     Floating or dockable panel that shows all CLI commands sent and received, live and in chronological order.
// - Implementation:
//     Can be a detachable window, popover, or sidebar panel. Should display timestamps, sent commands, responses, and errors. Optionally supports filtering or clearing the log.
//
// Note:
// Each view is modular; users can work in one area while keeping the CLI display visible for transparency and debugging. All views are kept synchronized with the parsed CLI state and error events.
//
//
// Implementation To-Do List
//
// - [ ] Create USBSerialManager for port discovery, probing, and CLI communication
// - [ ] Scaffold GuitarAccApp.swift with main app entry and navigation
// - [ ] Add stub views for: Status, Global Settings, Patch Config, MIDI Stats, Import/Export, Terminal, CLI Interaction Display
// - [ ] Wire up patch configuration view with patch selector and CLI-driven data flow (auto sync on appear + on change + on reconnect, export display, no default-to-0)
// - [ ] Create VirtualPortControl mixer-strip view with topology, function, and output controls
// - [ ] Integrate VirtualPortControlsSection into PatchView ControlPanelArea
// - [ ] Dynamic patch button count driven by device capability (queryPatchCount)
// - [ ] Implement device capability discovery sequence (topology count, function unit count, active patch load via loadActivePatchConfig)
// - [ ] Build CLIOutputParser — pure static parsing layer (ANSI strip, status, topo show, func show, JSON extraction, patch export, VirtualPortConfig derivation)
// - [ ] Define DeviceModels: PatchConfig, TopologyExport, FunctionExport as Codable/Equatable Swift structs
// - [ ] Parse patch export JSON into PatchConfig model struct (v2 firmware: includes topologies and functions arrays; optional fields for backward compat)
// - [ ] Bind parsed topology data to VirtualPortControl instances via parsedTopologyConfigs and virtualPortConfigs(from:)
// - [ ] Add unit test target (guitaraccUITests) with CLIOutputParserTests — all tests against verbatim device-captured strings
// - [ ]] Simplify sidebar to Patch Config + Terminal; move Status/Settings/MIDI Stats/Import/Export to PatchHeaderArea modal sheets
// - [ ] Bind parsed patch data to AccelerometerControl instances (firmware updated: accel_scale[6] and accel_offset[6] now exported in patch JSON; AccelerometerControlsSection wired into ControlPanelArea)
// - [ ] Implement write-back: control edits issue CLI commands to device
// - [ ] Implement backend command controller for issuing CLI commands and parsing responses
// - [ ] Wire up global settings view with live data binding and CLI-driven updates (modal sheet)
// - [ ] Implement status and monitoring view with periodic updates (modal sheet)
// - [ ] Implement MIDI statistics and diagnostics view (modal sheet)
// - [ ] Complete configuration import/export view — split into separate Import and Export modal sheets
// - [ ] Build advanced/terminal view with full command entry and logging
// - [ ] Enhance CLI interaction display (filtering, clear log, detach)
// - [ ] Integrate robust error handling, notifications, and connection state UI
// - [ ] Polish UI layout, navigation, and macOS app details
//
// Note: This checklist will be updated as implementation progresses.

## Device Capability Discovery

### Overview
The GUI must not hardcode assumptions about device features. Instead, it queries the connected basestation on connection to discover its capabilities, and adapts the UI accordingly. When no device is connected, capability-driven UI elements are hidden or show a placeholder state.

### Discovery Timing
- **On successful connection**: Immediately after the CLI probe succeeds, the GUI runs a capability discovery sequence before populating any views.
- **On reconnection**: Re-runs the full discovery sequence; cached capabilities from a previous session are discarded.
- **On disconnection**: All capability values reset to zero/empty. Views revert to their disconnected state.

### Device-Level Capabilities (Queried Once Per Connection)
These are structural properties of the firmware that do not change when the user switches patches.

| Capability | CLI Command | What it controls in the GUI |
|---|---|---|
| **Patch count** | `config list` (count `Patch <N>` lines) | Number of patch buttons in PatchSelectionArea. Zero = no buttons, placeholder shown. |
| **Topology instance count** | `topo show` (count reported instances) | Number of VirtualPortControl mixer strips rendered in VirtualPortControlsSection. |
| **Function unit count** | `func show` (probe indices 0-7, count valid responses) | Range of function unit indices available in VirtualPortControl pickers. |
| **Supported topology types** | `topo show` (parse available types) | Options shown in the topology type picker within each VirtualPortControl. |
| **Supported mixer types** | Inferred from firmware (currently fixed at 5: Passthrough, Sum, Average, Max, Min) | Options in the mixer type picker for dual-input topologies. |
| **Accelerometer axis count** | Inferred from firmware (currently fixed at 6: X, Y, Z, Roll, Pitch, Yaw) | Number of AccelerometerControl instances and axis picker options. |
| **MIDI channel range** | Inferred from firmware (1-16) | MIDI channel picker range in global settings and per-control. |
| **Max CC value** | Inferred from firmware (0-127) | Knob and text field ranges for CC mapping. |
| **Firmware version** | `status` (parse version string) | Displayed in status view; may gate feature availability in future. |

### Capability Storage
- Capabilities are stored as `@Published` properties on `USBSerialManager` (e.g., `patchCount`, `topologyInstanceCount`, `functionUnitCount`).
- Views observe these properties and reactively show/hide controls.
- When a capability is zero or unknown, the corresponding UI section shows a disabled or placeholder state rather than broken controls.

### Discovery Sequence (Post-Connection)
1. `config list` -> set `patchCount`
2. `topo show` -> set `topologyInstanceCount`, parse available topology types
3. `func show` -> set `functionUnitCount`
4. `status` -> set `firmwareVersion`, connected device count, MIDI output state

### View Adjustments Driven by Device Capabilities

| View / Control | Disconnected State | Connected State |
|---|---|---|
| **PatchSelectionArea** | "No basestation connected" placeholder, no buttons, search hidden | Shows exactly `patchCount` patch buttons, search enabled |
| **VirtualPortControlsSection** | Hidden or collapsed | Renders `topologyInstanceCount` mixer strips |
| **VirtualPortControl topology picker** | N/A | Populated with discovered topology types only |
| **VirtualPortControl function picker** | N/A | Range limited to `0..<functionUnitCount` |
| **AccelerometerControlsSection** | Hidden or shows static defaults | Renders controls matching axis count |
| **Global Settings View** | Fields disabled, placeholder values | Fields enabled, populated from `config export global` |
| **Status View** | "Not connected" indicator | Live status from `status` command, firmware version shown |
| **MIDI Statistics View** | Counters at zero, refresh disabled | Live data from `midi rx_stats`, reset enabled |

## Patch Data Binding

### Overview
When the user selects a patch, the GUI fetches that patch's full configuration from the device and populates all controls in the ControlPanelArea with the patch's values. Edits flow back to the device via CLI commands. This section defines which data comes from the patch export and how it maps to controls.

### Data Source
- **Command**: `config export patch <n>` returns JSON for the selected patch.
- **Topology**: `topo show` returns topology configuration for the current patch (topology instances are per-patch).
- **Function units**: `func show <idx>` returns function unit parameters for the current patch context.

### Patch Export JSON Structure (Current)
```json
{
  "version": 1,
  "config": {
    "patches": [
      {
        "patch_num": 0,
        "patch_name": "Patch 0",
        "velocity_curve": 0,
        "cc_mapping": [16, 17, 18, 19, 20, 21],
        "led_mode": 0,
        "accel_deadzone": 1,
        "accel_min": [0, 0, 0, 0, 0, 0],
        "accel_max": [127, 127, 127, 127, 127, 127],
        "accel_invert": 0
      }
    ]
  }
}
```

### Topology State (Per-Patch, from `topo show`)
Each topology instance reports:
- Topology type (T1-T4)
- Input axis/axes
- Function unit index/indices
- MIDI CC output(s)
- Mixer type (per-patch, shared across all dual-input instances)

### Control-to-Data Mapping

#### VirtualPortControl (one per topology instance)
| Control | Data Source | Write-Back Command |
|---|---|---|
| Topology type picker | `topo show` instance type | `topo config <inst> <type> <accel> [func] [cc]` |
| Input axis 1 picker | `topo show` instance accel source | `topo config <inst> ...` |
| Input axis 2 picker (T2/T4) | `topo show` instance second accel | `topo config <inst> ...` |
| Mixer type picker | `topo show` mixer type | `topo mixer <type>` |
| Function index picker | `topo show` instance func index | `topo config <inst> ...` |
| Output min knob | `func show <idx>` out_min | `func linear <idx> <in_min> <in_max> <out_min> <out_max>` |
| Output max knob | `func show <idx>` out_max | `func linear <idx> ...` |
| MIDI CC 1 field | `topo show` instance midi_cc | `topo config <inst> ...` |
| MIDI CC 2 field (T3/T4) | `topo show` instance second cc | `topo config <inst> ...` |

#### AccelerometerControl (one per axis)
| Control | Data Source (Patch Export JSON) | Write-Back Command |
|---|---|---|
| MIDI CC (channel picker, currently) | `cc_mapping[axis_index]` | `config cc <axis> <cc_num>` |
| Min knob | `accel_min[axis_index]` | `config accel_min <axis> <val>` |
| Max knob | `accel_max[axis_index]` | `config accel_max <axis> <val>` |
| Invert toggle (future) | `accel_invert` bitmask bit for axis | `config accel_invert <axis> <0\|1>` |

#### Patch-Level Fields (PatchHeaderArea or dedicated section)
| Field | Data Source (Patch Export JSON) | Write-Back Command |
|---|---|---|
| Patch name | `patch_name` | Future: `config patch_name <n> <name>` |
| Velocity curve | `velocity_curve` | `config velocity_curve <val>` |
| LED mode | `led_mode` | Future: `config led_mode <val>` |
| Accel deadzone | `accel_deadzone` | Future: `config accel_deadzone <val>` |

### Patch Load Sequence (On Patch Selection)
1. Issue `config select <n>` to activate the patch on the device.
2. Issue `config export patch <n>` to get the patch JSON.
3. Parse JSON into a `PatchConfig` model struct.
4. Issue `topo show` to get topology state for the now-active patch.
5. Parse topology output into per-instance `VirtualPortConfig` structs.
6. For each referenced function unit, issue `func show <idx>` to get linear params.
7. Bind all parsed data to the corresponding UI controls.

### Patch Save Sequence (On User Edit)
- Individual control changes issue their write-back command immediately (or on explicit save, per user preference).
- After all changes, issue `config save` to persist to flash.
- Re-export to confirm the device accepted the values.

### Structured Model (Swift)
The parsed patch data should be held in a `PatchConfig` struct that mirrors the JSON:
```swift
struct PatchConfig {
    var patchNum: Int
    var patchName: String
    var velocityCurve: Int
    var ccMapping: [Int]        // 6 values
    var ledMode: Int
    var accelDeadzone: Int
    var accelMin: [Int]         // 6 values
    var accelMax: [Int]         // 6 values
    var accelInvert: Int        // bitmask
    var topologyConfigs: [VirtualPortConfig]  // from topo show
}
```

### Error and Disconnection Behavior
- If a patch load fails mid-sequence, the GUI retains the last successfully loaded data and shows a warning.
- If the device disconnects during editing, unsaved changes are preserved in the local model. On reconnect, the GUI can offer to re-push local state or re-pull from device.
- Validation: The GUI should validate values against known ranges before issuing write-back commands. Out-of-range values are rejected locally with inline feedback.

## PatchView Layout and Scrolling

To ensure consistent terminology and implementation across the app, PatchView is organized into three top-level vertical areas, followed by feature sections within the control panel.

### Top-Level Areas (Vertical Order)
1. PatchHeaderArea
   - Purpose: Global actions and device info access. Contains two groups of action buttons separated by dividers, plus undo/redo at the trailing end.
   - Button layout (left to right):
     - **Save** | **Load** | [divider] | **Status** | **Settings** | **MIDI Stats** | [divider] | **Import** | **Export** | [spacer] | **Undo** | **Redo**
   - Save and Load use `Label` with SF Symbols (`square.and.arrow.down` / `square.and.arrow.up`) for file operations.
   - Import and Export use `Label` with SF Symbols (`arrow.down.doc` / `arrow.up.doc`) and match the Save/Load button style.
   - Status, Settings, MIDI Stats, Import, and Export each present a `.sheet` modal when tapped. These views were previously listed as sidebar navigation items and have been moved here as modal sheets.
   - Behavior: Fixed at the top of PatchView. Background uses `.thinMaterial` with rounded corners.

2. PatchSelectionArea
   - Purpose: Patch browsing and selection. Includes search, filters, banks/categories, and favorites.
   - Behavior: Updates the active patch context that drives the rest of the UI.

3. ControlPanelArea
   - Purpose: Houses all performance and configuration controls (initially accelerometer controls; expandable to modulation, effects, routing, etc.).
   - Behavior: Typically scrollable. Supports horizontal sections for wide sets of controls.

### Control Panel Sections
- VirtualPortControlsSection: Horizontal array of VirtualPortControl mixer strips (one per topology instance, count driven by device capability discovery).
- AccelerometerControlsSection: Contains multiple accelerometer modules (count driven by device axis count, typically 6).
- ModulationControlsSection: LFOs, envelopes, modulation routing (future).
- EffectsControlsSection: Reverb, delay, distortion, etc. (future).
- RoutingControlsSection: Signal routing and mapping (future).

Each Section may optionally contain one or more Groups (e.g., AccelerometerControlsGroup) when sub-clustering is useful.

## Scrolling Strategy

The PatchView supports scrolling to adapt to content that exceeds the available space. Use the following patterns:

1) Vertical scroll for entire PatchView
- Wrap the full vertical stack in `ScrollView(.vertical)`.
- Show indicators via `.scrollIndicators(.visible)` when appropriate.
- Recommended when the combined height of header, selection, and control panel can overflow.

2) Static header + scrolling content
- Keep `PatchHeaderArea` and `PatchSelectionArea` fixed in a parent `VStack`.
- Wrap `ControlPanelArea` in a `ScrollView(.vertical)` to allow the main content to scroll independently.
- Useful when the control area is the primary growth region.

3) Horizontal scrolling inside ControlPanelArea
- For wide collections (e.g., many accelerometer modules), use `ScrollView(.horizontal)` around a horizontal stack of modules.
- Combine with a vertical scroll at the top level, if needed.

4) Bidirectional overflow (advanced)
- Prefer vertical scrolling at the top level and horizontal scrolling within sections to avoid gesture conflicts.
- Avoid nesting scroll views with the same axis.

### Platform Considerations
- iOS/iPadOS:
  - Use `.scrollIndicators(.visible)` for discoverability.
  - Prefer material backgrounds (e.g., `.thinMaterial`, `.ultraThinMaterial`) for layered look.
- macOS:
  - System scrollbars appear automatically on scroll; `.scrollIndicators(.visible)` is still supported.
  - Consider `.pickerStyle(.menu)` or `.segmented)` instead of wheel styles.

## Accessibility and UI Testing Identifiers
Use stable identifiers derived from the same vocabulary:
- PatchHeaderArea.SaveButton
- PatchSelectionArea.SearchField
- ControlPanelArea.AccelerometerControlsSection.AccelerometerControl.<index>

These identifiers should be applied via `.accessibilityIdentifier("…")` where applicable.

## View and ViewModel Naming Conventions
- Views: PatchHeaderArea, PatchSelectionArea, ControlPanelArea, AccelerometerControlsSection
- ViewModels: PatchHeaderViewModel, PatchSelectionViewModel, ControlPanelViewModel, AccelerometerControlsViewModel

This naming scheme aligns code, documentation, and team communication.

## Example Structure (SwiftUI Sketch)
```swift
struct PatchView: View {
    var body: some View {
        VStack(spacing: 12) {
            PatchHeaderArea()
                .accessibilityIdentifier("PatchHeaderArea")

            PatchSelectionArea()
                .accessibilityIdentifier("PatchSelectionArea")

            // Either scroll the entire area here, or scroll within sections
            ScrollView(.vertical) {
                ControlPanelArea()
                    .accessibilityIdentifier("ControlPanelArea")
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal)
            }
            .scrollIndicators(.visible)
        }
        .padding()
    }
}

struct ControlPanelArea: View {
    var body: some View {
        ScrollView(.horizontal) {
            HStack(spacing: 16) {
                AccelerometerControlsSection()
                // Future: ModulationControlsSection(), EffectsControlsSection(), etc.
            }
            .padding(.vertical, 8)
            .padding(.horizontal, 12)
        }
        .scrollIndicators(.visible)
        .background(.ultraThinMaterial)
        .clipShape(RoundedRectangle(cornerRadius: 8))
    }
}


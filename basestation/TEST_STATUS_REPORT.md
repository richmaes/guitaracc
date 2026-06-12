# Test Scripts Status Report

Generated: 2026-06-12  
**Updated**: Tests have been reorganized into directories

## Test Organization

### Working Tests (basestation/)
Tests that work with current firmware - **5 scripts remain in basestation/**

### Non-Working Tests (basestation/cli_tests/non-working/)
Tests needing minor updates - **5 scripts moved to cli_tests/non-working/**

### Broken Tests (basestation/cli_tests/broken/)
Tests requiring major rewrites - **2 scripts moved to cli_tests/broken/**

See README files in each directory for details on fixes needed.

---

## Summary

Total test scripts: 12

- ✅ **Likely Working**: 5 scripts (in basestation/)
- ⚠️ **Needs Updates**: 5 scripts (in cli_tests/non-working/)
- 🔧 **Broken/Outdated**: 2 scripts (in cli_tests/broken/)

## Individual Test Status

### ✅ test_config.py
**Status**: Likely PASS  
**Description**: Tests `config midi_ch` command  
**Commands Used**:
- `config midi_ch 2` - ✅ EXISTS

**Notes**: Simple test, uses valid command. Uses select_port for auto-selection.

---

### ✅ test_persistence.py
**Status**: Likely PASS  
**Description**: Tests configuration persistence across power cycles  
**Commands Used**:
- `config show` - ✅ EXISTS
- `config midi_ch 5` - ✅ EXISTS

**Notes**: Tests that changes are saved to flash. Requires manual power cycle verification.

---

### ✅ test_shell.py
**Status**: Likely PASS  
**Description**: Basic shell connectivity test  
**Commands Used**:
- `status` - ✅ EXISTS

**Notes**: Simple connectivity and status command test.

---

### ✅ test_shell_direct.py
**Status**: Likely PASS  
**Description**: Direct serial test of status command  
**Commands Used**:
- `status` - ✅ EXISTS

**Notes**: Another basic connectivity test.

---

### ✅ test_midi_rx.py
**Status**: Likely PASS  
**Description**: Tests MIDI receive statistics  
**Commands Used**:
- `midi rx_stats` - ✅ EXISTS

**Notes**: Tests MIDI input monitoring.

---

### ⚠️ test_shell_simple.py
**Status**: NEEDS UPDATE (hardcoded port)  
**Description**: Simple shell test with hardcoded port  
**Commands Used**:
- `status` - ✅ EXISTS

**Issues**:
- **Hardcoded port**: `/dev/tty.usbmodem0010501494421` (device not connected)
- Port should be updated or use `select_port()`

**Fix**: Update port or switch to `select_port()` module

---

### ⚠️ test_config_commands.py
**Status**: NEEDS REVIEW (simple but hardcoded port)  
**Description**: Tests various config commands  
**Commands Used**:
- `config show` - ✅ EXISTS
- `config midi_ch 3` - ✅ EXISTS
- `config cc x 20` - ❌ **NOT REGISTERED** (function exists but not in shell command list)
- `config show` - ✅ EXISTS

**Issues**:
- **Hardcoded port**: `/dev/tty.usbmodem0010501494421`
- `config cc` command exists as function but NOT registered in shell commands

**Fix**: 
1. Update port or use `select_port()`
2. Remove `config cc` test or re-implement command registration

---

### ⚠️ test_json_output.py
**Status**: NEEDS REVIEW  
**Description**: Tests JSON output from pipeline commands  
**Commands Expected**:
- `pipeline json` - ✅ EXISTS
- `pipeline_monitor json` - ❓ UNKNOWN (need to verify if separate command)

**Notes**: Uses `select_port()` which is good. May need to verify `pipeline_monitor` command exists.

---

### ⚠️ test_accel_deadzone.py
**Status**: NEEDS UPDATE  
**Description**: Interactive test for accelerometer deadzone feature  
**Commands Used**:
- `config accel_deadzone <value>` - ✅ EXISTS
- Various monitoring commands

**Issues**:
- Interactive test requiring user input
- Requires BLE-connected client (Thingy:53)
- Cannot run automatically

**Notes**: Uses `select_port()` which is good. Test design is interactive by nature.

---

### 🔧 test_ui.py
**Status**: BROKEN - Commands Removed  
**Description**: UI interface testing tool  
**Commands Expected**:
- `config unlock_default` - ❌ **DOES NOT EXIST**
- `config write_default` - ❌ **DOES NOT EXIST**
- `config erase_all` - ✅ EXISTS

**Issues**:
- Old configuration commands have been removed/renamed
- Likely from older firmware version

**Fix**: Update to use current config commands:
- Replace `unlock_default` with current equivalents
- Replace `write_default` with `config save`

---

### 🔧 test_pipeline_monitor.py
**Status**: BROKEN - Command Removed  
**Description**: Real-time pipeline monitoring test  
**Commands Expected**:
- `accel_pipeline_monitor enable` - ❌ **DOES NOT EXIST**
- `accel_pipeline_monitor disable` - ❌ **DOES NOT EXIST**

**Issues**:
- `accel_pipeline_monitor` command has been removed
- Replaced by: `monitor` and `monitor json` commands

**Fix**: Rewrite to use:
- `monitor` - Real-time pipeline snapshot
- `monitor json` - JSON format output

---

### 🔧 test_midi_rt_tx.py
**Status**: NEEDS UPDATE (hardcoded port)  
**Description**: Tests MIDI real-time message transmission  
**Commands Used**:
- `midi send_rt 0xFA` (Start) - ✅ EXISTS
- `midi send_rt 0xF8` (Clock) - ✅ EXISTS
- `midi send_rt 0xFC` (Stop) - ✅ EXISTS

**Issues**:
- **Hardcoded port**: `/dev/tty.usbmodem0010501494421`

**Fix**: Update port or switch to `select_port()` module

---

## Commands Comparison

### Current Shell Commands (from source code)
```
status
monitor [json]
config show
config save
config restore
config patch <0-15>
config select <0-15>
config list
config midi_ch <1-16>
config accel_deadzone <0-127>
config velocity_curve <0-127>
config scan_interval <10-1000>
config avg_enable <0|1>
config avg_depth <3-10>
config export [global | patch <N>]
config import
config erase_all
midi rx_stats
midi rx_reset
midi program [0-127]
midi send_rt <0xF8-0xFF>
pipeline set <rho> <theta> <cc> <func> [params...]
pipeline show
pipeline json
```

### Legacy Commands (Found in Tests but NOT in Current Firmware)
```
config unlock_default  - REMOVED
config write_default   - REMOVED
config cc <x|y|z> <N>  - Function exists but NOT registered
accel_pipeline_monitor enable/disable - REMOVED (replaced by 'monitor')
```

## Recommendations

1. **Update Hardcoded Ports**: 
   - test_shell_simple.py
   - test_config_commands.py
   - test_midi_rt_tx.py
   - All should use `select_port()` module

2. **Rewrite Broken Tests**:
   - test_ui.py - Update to current config commands
   - test_pipeline_monitor.py - Rewrite to use `monitor` command

3. **Fix config cc Command**:
   - Either register `config cc` in shell commands
   - Or remove test from test_config_commands.py
   - **Note**: Old axis-based mapping has been replaced by rotation pipeline

4. **Document Interactive Tests**:
   - test_accel_deadzone.py requires manual interaction
   - Clearly mark as manual test in documentation

5. **Port Auto-Selection**:
   - Most tests already use `select_port()` ✓
   - Update remaining tests to use it

## Testing Recommendations

### Automated Testing
Can be automated:
- test_config.py
- test_persistence.py (with power cycle automation)
- test_shell.py
- test_shell_direct.py
- test_midi_rx.py
- test_midi_rt_tx.py (after port fix)
- test_json_output.py

### Manual/Interactive Testing
Requires human interaction:
- test_accel_deadzone.py (requires moving device)
- test_ui.py (once fixed - interactive terminal)

### Needs Major Updates
- test_pipeline_monitor.py - Command structure changed
- test_config_commands.py - Some commands removed/changed

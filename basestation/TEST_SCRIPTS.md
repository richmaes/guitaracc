# Basestation Test Scripts

This directory contains various test and verification scripts for the GuitarAcc basestation firmware.

## Regression Test Suite

### Quick Start
Run the full regression test suite:
```bash
./run_regression_tests.sh
```

This will execute all verification tests in sequence and report overall pass/fail status.

## Individual Test Scripts

### 1. Patch Count & Persistence Test
**Script:** `verify_patch_count.py`

Tests the 16-patch configuration system:
- Validates patch selection boundaries (0-15 valid, 16+ rejected)
- Writes unique random data to all 16 patches
- Verifies data persistence across saves/loads
- Tests random patch switching
- Validates CC mappings, velocity curves, min/max/invert for all axes

**Run:**
```bash
./verify_patch_count.py
```

**What it tests:**
- ✓ Patch selection boundary validation
- ✓ Configuration data persistence in flash
- ✓ Patch isolation (no cross-contamination)
- ✓ All configuration fields: CC mapping, velocity curve, accel min/max/invert
- ✓ Patch switching integrity

### 2. Configuration Export/Import Test
**Script:** `verify_export_import.py`

Tests the JSON-based config export/import functionality:
- Validates full configuration export (global + all 16 patches)
- Tests global-only export
- Tests single-patch export
- Verifies JSON structure and field presence
- Validates field value ranges
- Tests export consistency (multiple exports return same data)
- Tests file save/reload

**Run:**
```bash
./verify_export_import.py
```

**What it tests:**
- ✓ Full configuration export (JSON format)
- ✓ Global-only export
- ✓ Single-patch export
- ✓ JSON structure validation
- ✓ Field range validation (MIDI channel, CC values, etc.)
- ✓ Export consistency
- ✓ File save/reload

## Helper Scripts

### config_tool.py
Python utility for configuration management via serial port:

```bash
# Export full configuration
./config_tool.py export -p /dev/ttyUSB0 -o config.json

# Export global settings only
./config_tool.py export -t global -o global.json

# Export single patch
./config_tool.py export -t patch --patch 5 -o patch5.json

# Validate configuration file
./config_tool.py validate -i config.json

# Import configuration (shows instructions)
./config_tool.py import -i config.json
```

### select_port.py
Helper module for automatic serial port selection. Used by other scripts to automatically detect and select the correct USB serial port.

## Test Organization

```
basestation/
├── run_regression_tests.sh       # Main regression test runner
├── verify_patch_count.py          # Test 1: Patch persistence
├── verify_export_import.py        # Test 2: Export/import
├── config_tool.py                 # Configuration management utility
├── select_port.py                 # Port selection helper
├── test_*.py                      # Other unit tests
└── test/                          # C unit tests
    ├── Makefile
    └── test_*.c
```

## Adding New Tests

To add a new test to the regression suite:

1. Create your test script (e.g., `verify_new_feature.py`)
2. Make it executable: `chmod +x verify_new_feature.py`
3. Follow the pattern:
   - Use `select_port` for automatic port selection
   - Return exit code 0 on success, 1 on failure
   - Print clear pass/fail messages
4. Add test to `run_regression_tests.sh`:
   ```bash
   run_test "New Feature" "./verify_new_feature.py" || true
   ```

## Test Script Template

```python
#!/usr/bin/env python3
import serial
import sys
from select_port import select_port

def main():
    port = select_port(auto_select=True)
    if port is None:
        print("No port selected. Exiting.")
        sys.exit(1)
    
    try:
        ser = serial.Serial(port, 115200, timeout=2, rtscts=True)
        print(f"Connected to {port}")
        
        # Your tests here
        all_passed = True
        
        # ... test logic ...
        
        ser.close()
        
        if all_passed:
            print("✓✓✓ ALL TESTS PASSED ✓✓✓")
            sys.exit(0)
        else:
            print("✗✗✗ SOME TESTS FAILED ✗✗✗")
            sys.exit(1)
            
    except Exception as e:
        print(f'Error: {e}')
        sys.exit(1)

if __name__ == "__main__":
    main()
```

## Requirements

- Python 3.x
- pyserial package: `pip install pyserial`
- Connected basestation device via USB
- Firmware flashed and running

## Continuous Integration

These tests can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions workflow
- name: Run Regression Tests
  run: |
    cd basestation
    ./run_regression_tests.sh
```

## Troubleshooting

**Port not found:**
- Check USB cable connection
- Verify device is powered on
- On Linux, check permissions: `sudo usermod -a -G dialout $USER`
- Use `./config_tool.py export` (without -p) to see available ports

**Tests timeout:**
- Increase timeout values in test scripts
- Check for firmware hangs (monitor RTT log)
- Verify baud rate is 115200

**Random failures:**
- Flash may be slow - increase wait times after config save
- Try running tests individually to isolate issues
- Check RTT log for flash write errors

## Related Documentation

- [UI_INTERFACE.md](../UI_INTERFACE.md) - Shell command reference
- [CONFIG_STORAGE.md](../CONFIG_STORAGE.md) - Configuration storage details
- [ARCHITECTURE.md](../ARCHITECTURE.md) - System architecture

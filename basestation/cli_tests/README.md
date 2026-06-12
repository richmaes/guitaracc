# CLI Test Scripts

This directory contains test scripts for the GuitarAcc basestation CLI interface.

## Running Tests

All test scripts support the `--port` flag to specify a serial port:
```bash
python3 test_shell.py --port /dev/tty.usbmodem123
```

If `--port` is not specified, the script will auto-detect available USB modem ports.

### Test Output Format

All tests produce standardized output:
- **Detailed execution output** during the test
- **Final status line** in the format:
  - `TEST PASSED: [test name]` (exit code 0)
  - `TEST FAILED: [test name] - [reason]` (exit code 1)

This format enables easy automation and integration with CI/CD pipelines.

## Organization

### Working Tests
Tests that work with current firmware:
- `test_config.py` - Configuration command tests
- `test_persistence.py` - Configuration persistence tests
- `test_shell.py` - Basic shell connectivity
- `test_shell_direct.py` - Direct serial test
- `test_midi_rx.py` - MIDI receive statistics
- `verify_patch_count.py` - Patch storage and persistence (automated)
- `verify_export_import.py` - Configuration export/import (automated)

### Utility Files
- `select_port.py` - Serial port auto-detection helper

### non-working/
Tests that need updates to work with current firmware:
- Hardcoded serial ports that need updating
- Commands that have changed syntax
- Interactive tests requiring manual intervention

See `non-working/README.md` for details.

### broken/
Tests that are outdated and need major rewrites:
- Commands that no longer exist
- Features that have been replaced
- Significant architectural changes

See `broken/README.md` for details.

## Test Status

For detailed status of all tests, see [TEST_STATUS_REPORT.md](../TEST_STATUS_REPORT.md)

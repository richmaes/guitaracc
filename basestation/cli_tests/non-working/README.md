# Non-Working Tests

These tests need updates to work with current firmware but are generally salvageable.

## Tests in This Directory

### test_shell_simple.py
**Issue**: Hardcoded port `/dev/tty.usbmodem0010501494421`  
**Fix**: Update port or use `select_port()` module  
**Commands**: All valid (`status`)

### test_config_commands.py
**Issue**: 
- Hardcoded port `/dev/tty.usbmodem0010501494421`
- Uses `config cc x 20` command that exists but is not registered

**Fix**: 
- Update port or use `select_port()`
- Remove `config cc` test or re-register command
- Note: Old axis mapping replaced by rotation pipeline

**Commands**: Most valid (`config show`, `config midi_ch`)

### test_json_output.py
**Issue**: May need verification of commands  
**Fix**: Verify `pipeline json` and monitoring commands work  
**Commands**: Uses `select_port()` ✓

### test_accel_deadzone.py
**Issue**: Interactive test requiring manual device movement  
**Fix**: Document as manual test, update if needed  
**Commands**: Valid (`config accel_deadzone`)  
**Note**: Requires BLE-connected Thingy:53

### test_midi_rt_tx.py
**Issue**: Hardcoded port `/dev/tty.usbmodem0010501494421`  
**Fix**: Update port or use `select_port()`  
**Commands**: All valid (`midi send_rt`)

## How to Fix

1. **Port Updates**: Replace hardcoded ports with:
   ```python
   from select_port import select_port
   port = select_port(auto_select=True)
   ```

2. **Test Individually**: After fixing port issues, test each script

3. **Interactive Tests**: Mark `test_accel_deadzone.py` as manual test in docs

## Moving Back to Parent

Once fixed and tested, move back to parent directory:
```bash
git mv cli_tests/non-working/test_name.py .
```

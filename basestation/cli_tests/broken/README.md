# Broken/Outdated Tests

These tests use commands that have been removed or significantly changed. They require major rewrites.

## Tests in This Directory

### test_ui.py
**Status**: BROKEN - Commands Removed  
**Missing Commands**:
- `config unlock_default` - Does not exist
- `config write_default` - Does not exist
- `config erase_all` - Still exists ✓

**Replacement Commands**:
- Use `config save` instead of `write_default`
- Update unlock mechanism if still needed

**Fix Required**: Major rewrite to use current configuration commands

---

### test_pipeline_monitor.py
**Status**: BROKEN - Command Removed  
**Missing Commands**:
- `accel_pipeline_monitor enable` - Does not exist
- `accel_pipeline_monitor disable` - Does not exist

**Replacement Commands**:
- `monitor` - Show real-time pipeline snapshot (human-readable)
- `monitor json` - Show pipeline snapshot in JSON format

**Fix Required**: Complete rewrite to use new monitoring approach

**Old Approach**:
```python
ser.write(b"accel_pipeline_monitor enable\n")
# Parse APM lines: "APM timestamp x y z ..."
```

**New Approach**:
```python
ser.write(b"monitor json\n")
# Parse JSON output with pipeline stages
```

---

## Architecture Changes

### Pipeline Monitoring
- **Old**: Continuous stream with `accel_pipeline_monitor enable/disable`
- **New**: On-demand snapshot with `monitor` or `monitor json`

### Configuration Commands
- **Old**: `unlock_default` / `write_default` pattern
- **New**: Simplified `config save` / `config restore`

### Accelerometer Mapping
- **Old**: Per-axis CC mapping (`config cc x 1`)
- **New**: Rotation pipeline (`pipeline set <rho> <theta> <cc> <func> [params]`)

## Rewriting vs. Archiving

**Consider Rewriting If**:
- Test functionality is still relevant
- New commands provide similar capabilities
- Test design is sound

**Consider Archiving If**:
- Feature has been completely removed
- New architecture makes test obsolete
- Better test coverage exists elsewhere

## Moving Back to Parent

After complete rewrite and testing:
```bash
git mv cli_tests/broken/test_name.py .
```

Or archive:
```bash
git mv cli_tests/broken/test_name.py cli_tests/archived/
```

---
name: guitaracc-test-script
description: 'Create Python test scripts for GuitarAcc basestation CLI interface. Use when: writing new test scripts, adding test cases, creating automated tests, validating serial commands, testing firmware functionality. Enforces --port flag, select_port.py usage, standardized pass/fail messaging, and exit codes.'
argument-hint: 'Brief description of what the test should verify'
---

# GuitarAcc Test Script Creation

Create standardized Python test scripts for the GuitarAcc basestation CLI interface that integrate with the automated regression test suite.

## When to Use

- Writing new test scripts for basestation CLI commands
- Adding automated test cases to the test suite
- Creating validation scripts for firmware features
- Testing serial communication and command responses
- Verifying configuration persistence or state changes

## Test Script Requirements

All GuitarAcc test scripts must follow these standards:

### 1. Import Structure

```python
#!/usr/bin/env python3
import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "Descriptive Test Name"
```

**Required imports:**
- `serial` - PySerial for serial communication
- `time` - Delays for command processing
- `sys` - Exit codes
- `argparse` - Command-line argument parsing
- `select_port` from `select_port.py` - Port auto-detection utility

**TEST_NAME constant:** Define a descriptive name used in pass/fail messages

### 2. Command-Line Arguments

```python
parser = argparse.ArgumentParser(description='Brief test description')
parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
args = parser.parse_args()
```

**Required:**
- `--port` flag for explicit port specification
- Fallback to `select_port()` when not specified

### 3. Port Selection Logic

```python
port = args.port if args.port else select_port(auto_select=True)
if port is None:
    print(f"TEST FAILED: {TEST_NAME} - No port selected")
    sys.exit(1)
```

**Behavior:**
- Use `--port` argument if provided
- Otherwise call `select_port(auto_select=True)` for auto-detection
- Exit with code 1 if no port available

### 4. Serial Communication

```python
try:
    print(f"Opening {port}...")
    ser = serial.Serial(port, 115200, timeout=1, rtscts=True)
    time.sleep(1)  # Allow connection to stabilize
    
    # Clear any stale data
    ser.reset_input_buffer()
    
    # Send command
    ser.write(b'command\r\n')
    ser.flush()
    time.sleep(0.5)  # Wait for response
    
    # Read response
    if ser.in_waiting:
        response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
        print(response)
    
    ser.close()
```

**Parameters:**
- Baud rate: `115200`
- Timeout: `1` second (adjust for slow commands)
- RTS/CTS flow control: `rtscts=True`
- Line endings: `\r\n` (carriage return + newline)

**Timing:**
- Initial connection: `time.sleep(1)` after opening
- Command response: `time.sleep(0.5)` or longer for complex commands
- Configuration saves: `time.sleep(0.6)` minimum

### 5. Response Validation

Don't just print responses—validate them:

```python
response_valid = False
if ser.in_waiting:
    response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
    print("Response:")
    print(response)
    
    # Check for expected content
    if "Expected String" in response or "success" in response.lower():
        response_valid = True
else:
    print("No response received")
```

**Validation strategies:**
- Check for expected keywords in response
- Verify response is non-empty
- Parse structured output (e.g., JSON)
- Validate numeric ranges or formats

### 6. Error Handling

Wrap all serial operations in try-except:

```python
try:
    # ... serial communication and validation ...
    
    # Determine result based on validation checks
    if all_checks_passed:
        print(f"\nTEST PASSED: {TEST_NAME}")
        sys.exit(0)
    else:
        reasons = ["check1 failed", "check2 failed"]
        print(f"\nTEST FAILED: {TEST_NAME} - {', '.join(reasons)}")
        sys.exit(1)
        
except Exception as e:
    print(f"\nTEST FAILED: {TEST_NAME} - {e}")
    sys.exit(1)
```

### 7. Standardized Output Messages

**Final status line format (REQUIRED):**

Success:
```
TEST PASSED: [TEST_NAME]
```

Failure:
```
TEST FAILED: [TEST_NAME] - [specific reason]
```

**Requirements:**
- Print detailed output during execution (commands sent, responses received)
- Always print a final status line at the end
- Include specific failure reason (not just "failed")
- Use the TEST_NAME constant for consistency

### 8. Exit Codes

**REQUIRED:**
- Exit code `0` for PASS
- Exit code `1` for FAIL
- Always use `sys.exit()` explicitly

**Example:**
```python
if test_successful:
    print(f"\nTEST PASSED: {TEST_NAME}")
    sys.exit(0)
else:
    print(f"\nTEST FAILED: {TEST_NAME} - {failure_reason}")
    sys.exit(1)
```

## Complete Template

```python
#!/usr/bin/env python3
import serial
import time
import sys
import argparse
from select_port import select_port

TEST_NAME = "Your Test Name Here"

def main():
    parser = argparse.ArgumentParser(description='Test description')
    parser.add_argument('--port', help='Serial port to use (e.g., /dev/tty.usbmodem123)')
    args = parser.parse_args()
    
    port = args.port if args.port else select_port(auto_select=True)
    if port is None:
        print(f"TEST FAILED: {TEST_NAME} - No port selected")
        sys.exit(1)
    
    try:
        print(f"Opening {port}...")
        ser = serial.Serial(port, 115200, timeout=1, rtscts=True)
        time.sleep(1)
        ser.reset_input_buffer()
        
        # Test Logic Here
        print("Sending command...")
        ser.write(b'your_command\r\n')
        ser.flush()
        time.sleep(0.5)
        
        # Validate Response
        test_passed = False
        if ser.in_waiting:
            response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
            print("Response:")
            print(response)
            
            # Add validation logic
            if "expected_output" in response:
                test_passed = True
        else:
            print("No response received")
        
        ser.close()
        
        # Report Result
        if test_passed:
            print(f"\nTEST PASSED: {TEST_NAME}")
            sys.exit(0)
        else:
            print(f"\nTEST FAILED: {TEST_NAME} - no valid response")
            sys.exit(1)
            
    except Exception as e:
        print(f"\nTEST FAILED: {TEST_NAME} - {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
```

## File Placement

Save test scripts in:
- **Working tests**: `basestation/cli_tests/` (tests that work with current firmware)
- **Non-working tests**: `basestation/cli_tests/non-working/` (tests needing minor updates)
- **Broken tests**: `basestation/cli_tests/broken/` (tests requiring major rewrites)

## Testing the Test

After creating a test script:

1. **Make executable** (optional):
   ```bash
   chmod +x test_name.py
   ```

2. **Test with explicit port**:
   ```bash
   python3 test_name.py --port /dev/tty.usbmodem123
   ```

3. **Test with auto-selection**:
   ```bash
   python3 test_name.py
   ```

4. **Verify exit code**:
   ```bash
   python3 test_name.py && echo "PASSED" || echo "FAILED"
   ```

5. **Check --help**:
   ```bash
   python3 test_name.py --help
   ```

## Integration with Regression Suite

To add to the automated regression test suite, edit `basestation/run_regression_tests.sh`:

```bash
# Add to appropriate section (Automated or Interactive)
run_test "Your Test Description" "./cli_tests/test_name.py" || true
```

The regression runner will:
- Execute your test script
- Capture the exit code
- Display colored output (green for pass, red for fail)
- Generate a summary report

## Common Patterns

### Testing Command Output

```python
# Send command and validate specific output
ser.write(b'config show\r\n')
time.sleep(0.5)
response = ser.read(ser.in_waiting).decode('utf-8', errors='replace')

if "MIDI channel:" in response and "Patch:" in response:
    print("✓ Config output valid")
    test_passed = True
```

### Testing Configuration Changes

```python
# Change a setting and verify
ser.write(b'config midi_ch 5\r\n')
time.sleep(0.3)
ser.read_all()  # Consume response

# Verify with show command
ser.write(b'config show\r\n')
time.sleep(0.3)
response = ser.read_all().decode('utf-8', errors='replace')

if "MIDI channel: 5" in response:
    print("✓ MIDI channel changed successfully")
    test_passed = True
```

### Multiple Validation Checks

```python
checks_passed = []

# Check 1
if condition1:
    checks_passed.append("condition1")
else:
    print("✗ Condition 1 failed")

# Check 2
if condition2:
    checks_passed.append("condition2")
else:
    print("✗ Condition 2 failed")

# Final result
if len(checks_passed) == 2:
    print(f"\nTEST PASSED: {TEST_NAME}")
    sys.exit(0)
else:
    failed = [c for c in ["condition1", "condition2"] if c not in checks_passed]
    print(f"\nTEST FAILED: {TEST_NAME} - {', '.join(failed)} failed")
    sys.exit(1)
```

## Example Test Scripts

Reference existing test scripts as examples:
- `basestation/cli_tests/test_shell.py` - Basic connectivity
- `basestation/cli_tests/test_config.py` - Single command test
- `basestation/cli_tests/test_persistence.py` - Configuration persistence
- `basestation/cli_tests/verify_patch_count.py` - Complex multi-step test
- `basestation/cli_tests/verify_export_import.py` - JSON validation

## Procedure

When asked to create a test script:

1. **Clarify the test scope**: What command(s) or feature should be tested?
2. **Determine validation criteria**: What constitutes success vs failure?
3. **Choose appropriate template**: Simple single-command or complex multi-step?
4. **Implement the test** following all requirements above
5. **Save to appropriate directory** (cli_tests/ or subdirectory)
6. **Verify the script** runs with --help flag
7. **Document** what the test does (if adding to suite)

## Quality Checklist

Before considering a test script complete, verify:

- [ ] Imports `select_port` from `select_port.py`
- [ ] Defines `TEST_NAME` constant
- [ ] Accepts `--port` command-line argument
- [ ] Falls back to `select_port(auto_select=True)`
- [ ] Uses 115200 baud rate with rtscts=True
- [ ] Validates responses (not just printing)
- [ ] Prints detailed output during execution
- [ ] Prints final status: "TEST PASSED" or "TEST FAILED"
- [ ] Includes specific failure reason in failed message
- [ ] Exits with code 0 on success, 1 on failure
- [ ] Wraps serial operations in try-except
- [ ] Handles Exception with proper TEST FAILED message
- [ ] Works with `--port` flag
- [ ] Works without arguments (auto-select)
- [ ] Responds to `--help` flag

## Notes

- The `select_port.py` utility is located in `basestation/cli_tests/`
- All test scripts should be in same directory as `select_port.py` or import path adjusted
- Basestation serial settings: 115200 baud, 8N1, RTS/CTS flow control
- Commands require `\r\n` line endings
- Allow settling time after opening serial port (1 second)
- Use `ser.reset_input_buffer()` to clear stale data before testing

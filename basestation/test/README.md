# Host-Based Testing for Basestation

This directory contains host-based tests that run on your development machine (macOS/Linux/Windows) to verify the logic of embedded functions before flashing to hardware.

## ⚠️ Important: Single Source of Truth

**The tests use the ACTUAL embedded source code** - no duplication!

```
test/test_midi_cc.c  ----compiles---->  ../src/midi_logic.c
                                              ↑
basestation/main.c   ----includes---->  midi_logic.h
```

When you test on your Mac, you're testing the **exact same code** that runs on the nRF5340. This ensures:
- ✅ No code duplication or drift
- ✅ Tests validate real implementation
- ✅ Changes to logic automatically tested
- ✅ Single point of maintenance

## Why Host-Based Testing?

**Common Practice:** Testing on the host machine is a standard embedded development practice that offers:

- **Fast iteration**: Compile and run in seconds vs. flash/debug cycle
- **Easy debugging**: Use native debuggers (lldb, gdb) and print statements
- **CI/CD integration**: Run tests automatically in build pipelines
- **Test-driven development**: Write tests before implementing on hardware
- **No hardware needed**: Test logic without physical devices

## Quick Start

```bash
cd /Users/richmaes/src/guitaracc/basestation/test

# Build and run tests
make test

# Or step by step:
make            # Build
./test_midi_cc  # Run
```

## Test Coverage

### `test_midi_cc.c`
Tests the MIDI CC message construction functions:
- `accel_to_midi_cc()` - Converts accelerometer values to MIDI CC values (0-127)
- `construct_midi_cc_msg()` - Builds MIDI CC message bytes

**Test Cases:**
1. Boundary value testing (0g, ±2g)
2. Range clamping (values outside ±2g)
3. MIDI message format validation
4. Channel and CC number masking
5. Complete end-to-end flow with realistic inputs

## Adding New Tests

### 1. Copy Functions from Embedded Code
Extract the pure logic functions (no hardware dependencies) from `src/main.c`:

```c
// Functions that can be tested on host:
static uint8_t accel_to_midi_cc(int16_t milli_g) { ... }
```

### 2. Create Test Functions
```c
static void test_my_function(void) {
    // Arrange
    int input = 100;
    
    // Act
    int result = my_function(input);
    
    // Assert
    assert_equal_int("My test", expected, result);
}
```

### 3. Add to Test Runner
```c
int main(void) {
    test_accel_to_midi_conversion();
    test_my_function();  // Add here
    // ...
}
```

## Best Practices

### ✅ DO
- Test pure functions (no side effects)
- Mock hardware dependencies (UART, timers, etc.)
- Test boundary conditions and edge cases
- Keep tests independent and repeatable
- Use descriptive test names

### ❌ DON'T
- Test Zephyr RTOS functions directly (use mocks)
- Depend on timing or hardware state
- Mix test logic with production code
- Skip error cases

## Formalizing the Process

To make host-based testing standard practice:

### Project Structure
```
basestation/
├── src/
│   └── main.c              # Embedded code
├── test/
│   ├── test_midi_cc.c      # Host tests
│   ├── test_utils.h        # Shared test utilities
│   ├── mocks/              # Hardware mocks
│   │   └── mock_uart.c
│   ├── Makefile            # Build system
│   └── README.md           # This file
└── CMakeLists.txt          # Zephyr build
```

### Workflow Integration
1. **Before Writing Code**: Write failing tests (TDD)
2. **During Development**: Run `make test` frequently
3. **Before Committing**: Ensure all tests pass
4. **In CI/CD**: Run tests automatically on every push

### Example Development Cycle
```bash
# 1. Write a new test (it fails)
vim test/test_midi_cc.c
make test  # ❌ Fails as expected

# 2. Implement the function in src/main.c
vim src/main.c

# 3. Copy to test file and verify
make test  # ✅ Passes

# 4. Flash to hardware for integration testing
cd ..
west build -b nrf5340_audio_dk_nrf5340_cpuapp
west flash
```

## Continuous Integration Example

```yaml
# .github/workflows/test.yml
name: Host Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run host tests
        run: |
          cd basestation/test
          make test
```

## Tools and Extensions

- **Debugger**: `lldb ./test_midi_cc` or `gdb ./test_midi_cc`
- **Memory checks**: `valgrind ./test_midi_cc` (Linux)
- **Coverage**: `gcc --coverage` + `gcov`
- **Profiling**: `instruments -t Time` (macOS) or `gprof`

## References

- [Embedded Artistry: Unit Testing](https://embeddedartistry.com/blog/2017/05/08/unit-testing-basics/)
- [Test-Driven Development for Embedded C](https://pragprog.com/titles/jgade/test-driven-development-for-embedded-c/)
- [CMock: Mocking Framework](https://www.throwtheswitch.org/cmock)

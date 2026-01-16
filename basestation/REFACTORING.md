# Code Refactoring Summary: Single Source of Truth

## What Changed

### Before Refactoring ❌
```
src/main.c                          test/test_midi_cc.c
├─ accel_to_midi_cc()              ├─ accel_to_midi_cc()      [DUPLICATE]
├─ construct_midi_cc_msg()         ├─ construct_midi_cc_msg() [DUPLICATE]
└─ send_midi_cc()                  └─ (testing duplicates)
```

**Problems:**
- Code duplication - functions copied between files
- Maintenance burden - changes must be made twice
- Drift risk - test and production code can diverge
- Not testing actual production code

### After Refactoring ✅
```
src/midi_logic.h                    src/midi_logic.c
├─ Function declarations           ├─ accel_to_midi_cc()
├─ Struct definitions               └─ construct_midi_cc_msg()
└─ Constants                                    ↑
        ↑                                      |
        |                                      |
        +-- included by --+                    |
                          |                    |
            src/main.c    |    test/test_midi_cc.c
            ├─ includes   |    ├─ includes
            └─ send_midi_cc()  └─ compiles with
               (uses logic)        (tests actual code)
```

**Benefits:**
- ✅ Single source of truth
- ✅ Tests validate real production code
- ✅ Changes automatically propagate
- ✅ Clear separation: logic vs. platform code

## File Structure

### New Files Created

1. **[src/midi_logic.h](../src/midi_logic.h)** (49 lines)
   - Pure interface definitions
   - No hardware dependencies
   - Can be included anywhere

2. **[src/midi_logic.c](../src/midi_logic.c)** (36 lines)
   - Pure business logic
   - Platform-agnostic
   - Testable on host

### Modified Files

1. **[src/main.c](../src/main.c)**
   - Added: `#include "midi_logic.h"`
   - Removed: Duplicate `accel_to_midi_cc()` function
   - Removed: Duplicate `struct accel_data` definition
   - Removed: Duplicate MIDI CC constants
   - Updated: `send_midi_cc()` to call `construct_midi_cc_msg()`

2. **[test/test_midi_cc.c](test_midi_cc.c)**
   - Added: `#include "../src/midi_logic.h"`
   - Removed: All duplicated functions (32 lines removed!)
   - Now compiles with actual source

3. **[test/Makefile](Makefile)**
   - Added: `../src/midi_logic.c` to sources
   - Added: `-I../src` to include path

4. **[CMakeLists.txt](../CMakeLists.txt)**
   - Added: `src/midi_logic.c` to embedded build

## What Gets Tested

### Functions in midi_logic.c
- `accel_to_midi_cc()` - Accelerometer to MIDI conversion
- `construct_midi_cc_msg()` - MIDI message byte construction

### Platform Code (NOT in midi_logic.c)
- `send_midi_cc()` in main.c - Uses Zephyr UART (hardware-dependent)
- Bluetooth/GATT code - Requires real hardware
- Test mode functions - Zephyr-specific

## Design Principle: Separation of Concerns

```
┌─────────────────────────────────────────┐
│  Business Logic (midi_logic.c)          │
│  - Pure functions                       │
│  - No hardware dependencies             │
│  - Testable on host                     │
│  - Single source of truth               │
└─────────────────────────────────────────┘
              ↑ used by
┌─────────────────────────────────────────┐
│  Platform Code (main.c)                 │
│  - Zephyr RTOS calls                    │
│  - Hardware access (UART, BLE)          │
│  - Logging, threading                   │
│  - Integration logic                    │
└─────────────────────────────────────────┘
```

## Testing Workflow

### 1. Development on Mac
```bash
cd test/
make test          # Tests actual production code in seconds
```

### 2. Deploy to Hardware
```bash
cd ../
west build         # Compiles same code for nRF5340
west flash         # Flash to device
```

### 3. Confidence
- If host tests pass, embedded logic is correct
- Only integration issues remain to debug on hardware
- Dramatically reduces flash/debug cycles

## Adding New Testable Functions

### Step 1: Add to midi_logic.h
```c
/**
 * Your new pure function
 */
uint8_t my_new_function(int input);
```

### Step 2: Implement in midi_logic.c
```c
uint8_t my_new_function(int input)
{
    // Pure logic, no hardware calls
    return input * 2;
}
```

### Step 3: Test in test_midi_cc.c
```c
void test_my_new_function(void)
{
    assert_equal_uint8("Test case", 20, my_new_function(10));
}
```

### Step 4: Use in main.c
```c
#include "midi_logic.h"

void some_embedded_function(void)
{
    uint8_t result = my_new_function(5);
    uart_poll_out(uart, result);  // Platform-specific code
}
```

## Guidelines for Separation

### ✅ Put in midi_logic.c (testable)
- Mathematical calculations
- Data transformations
- Protocol formatting
- State machines (if pure)
- Lookup tables

### ❌ Keep in main.c (platform-specific)
- Zephyr kernel calls (k_sleep, k_thread_create, etc.)
- Hardware access (UART, SPI, I2C, GPIO)
- Bluetooth/GATT operations
- Logging (LOG_INF, LOG_DBG)
- Device tree accesses

## Verification

Run the tests to verify everything works:
```bash
cd test/
make clean
make test
```

Expected output:
```
✓ Build complete: ./test_midi_cc
✓ Using source: ../src/midi_logic.c
...
✅ ALL TESTS PASSED
```

Then verify embedded build (if west is configured):
```bash
cd ../
west build -b nrf5340_audio_dk_nrf5340_cpuapp
```

## Benefits Achieved

1. **No Code Duplication**: Logic exists in exactly one place
2. **Test Confidence**: Tests validate actual production code
3. **Maintainability**: Change once, tested everywhere
4. **Fast Iteration**: Test in milliseconds on host
5. **Clear Architecture**: Logic separated from platform
6. **Scalability**: Easy to add more testable functions

## Next Steps

Consider extracting more testable logic:
- [ ] Bluetooth device filtering logic
- [ ] Connection management state machine
- [ ] Data validation functions
- [ ] Configuration parsing

Each extraction makes your codebase more testable and maintainable!

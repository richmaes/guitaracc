/*
 * VALIDATION LOGIC EXPLAINED
 * 
 * This document explains where test validation happens and how expected
 * values were calculated for the MIDI CC tests.
 */

/* ============================================================================
 * VALIDATION FUNCTIONS (Lines 76-103 in test_midi_cc.c)
 * ============================================================================ */

// 1. assert_equal_uint8() - Validates single uint8_t values
//    Location: Lines 76-87
//    Usage: assert_equal_uint8("Test name", expected_value, actual_value)
//    
//    Example from line 115:
//      assert_equal_uint8("Zero acceleration (0g)", 63, accel_to_midi_cc(0));
//                                                   ^^  ^^^^^^^^^^^^^^^^^^^
//                                                   |         |
//                                                Expected   Actual (computed)

static void assert_equal_uint8(const char *test_name, uint8_t expected, uint8_t actual)
{
	stats.total_tests++;
	if (expected == actual) {
		printf("  ✓ %s: %d (0x%02X)\n", test_name, actual, actual);
		stats.passed_tests++;
	} else {
		// VALIDATION FAILS HERE - Shows what was expected vs actual
		printf("  ✗ %s: expected %d (0x%02X), got %d (0x%02X)\n", 
		       test_name, expected, expected, actual, actual);
		stats.failed_tests++;
	}
}

// 2. assert_midi_message() - Validates complete 3-byte MIDI messages
//    Location: Lines 89-103
//    Usage: assert_midi_message("Test", exp_byte0, exp_byte1, exp_byte2, actual_msg)
//
//    Example from line 139:
//      construct_midi_cc_msg(0, MIDI_CC_X_AXIS, 64, msg);
//      assert_midi_message("Channel 0, CC 16, value 64", 0xB0, 0x10, 0x40, msg);
//                                                        ^^^^  ^^^^  ^^^^  ^^^
//                                                        Expected bytes    Actual

static void assert_midi_message(const char *test_name, uint8_t exp_status, uint8_t exp_data1, 
                                 uint8_t exp_data2, uint8_t *actual)
{
	stats.total_tests++;
	// VALIDATION HAPPENS HERE - Compares all 3 bytes
	bool pass = (actual[0] == exp_status && actual[1] == exp_data1 && actual[2] == exp_data2);
	
	if (pass) {
		printf("  ✓ %s: [0x%02X 0x%02X 0x%02X]\n", test_name, actual[0], actual[1], actual[2]);
		stats.passed_tests++;
	} else {
		// VALIDATION FAILS HERE - Shows expected vs actual for each byte
		printf("  ✗ %s:\n", test_name);
		printf("    Expected: [0x%02X 0x%02X 0x%02X]\n", exp_status, exp_data1, exp_data2);
		printf("    Got:      [0x%02X 0x%02X 0x%02X]\n", actual[0], actual[1], actual[2]);
		stats.failed_tests++;
	}
}

/* ============================================================================
 * HOW EXPECTED VALUES WERE CALCULATED
 * ============================================================================ */

/* -----------------------------------------------------------------------------
 * 1. ACCELEROMETER TO MIDI CC CONVERSION
 * 
 * Formula from main.c (lines 68-77):
 *   value = ((int32_t)milli_g + 2000) * 127 / 4000
 *   Then clamped to [0, 127]
 * 
 * Derivation:
 *   - Input range:  -2000 to +2000 milli-g (±2g)
 *   - Output range: 0 to 127 (MIDI CC range)
 *   - Center point: 0g should map to ~64 (middle of MIDI range)
 * ----------------------------------------------------------------------------- */

// CALCULATION EXAMPLES:

// Test Case 1: Zero acceleration (0g)
//   Input: 0 milli-g
//   Formula: (0 + 2000) * 127 / 4000 = 2000 * 127 / 4000 = 254000 / 4000 = 63.5
//   Integer division: 63
//   Expected: 63
//   Validation: assert_equal_uint8("Zero acceleration (0g)", 63, accel_to_midi_cc(0));

// Test Case 2: Max positive (+2g)
//   Input: 2000 milli-g
//   Formula: (2000 + 2000) * 127 / 4000 = 4000 * 127 / 4000 = 508000 / 4000 = 127
//   Expected: 127
//   Validation: assert_equal_uint8("Max positive (+2000mg)", 127, accel_to_midi_cc(2000));

// Test Case 3: Max negative (-2g)
//   Input: -2000 milli-g
//   Formula: (-2000 + 2000) * 127 / 4000 = 0 * 127 / 4000 = 0
//   Expected: 0
//   Validation: assert_equal_uint8("Max negative (-2000mg)", 0, accel_to_midi_cc(-2000));

// Test Case 4: +1g (half of max positive)
//   Input: 1000 milli-g
//   Formula: (1000 + 2000) * 127 / 4000 = 3000 * 127 / 4000 = 381000 / 4000 = 95.25
//   Integer division: 95
//   Expected: 95
//   Validation: assert_equal_uint8("+1g (1000mg)", 95, accel_to_midi_cc(1000));

// Test Case 5: -1g (half of max negative)
//   Input: -1000 milli-g
//   Formula: (-1000 + 2000) * 127 / 4000 = 1000 * 127 / 4000 = 127000 / 4000 = 31.75
//   Integer division: 31
//   Expected: 31
//   Validation: assert_equal_uint8("-1g (-1000mg)", 31, accel_to_midi_cc(-1000));

// Test Case 6: +0.5g (quarter of max positive)
//   Input: 500 milli-g
//   Formula: (500 + 2000) * 127 / 4000 = 2500 * 127 / 4000 = 317500 / 4000 = 79.375
//   Integer division: 79
//   Expected: 79
//   Validation: assert_equal_uint8("+0.5g (500mg)", 79, accel_to_midi_cc(500));

// Test Case 7: -0.5g
//   Input: -500 milli-g
//   Formula: (-500 + 2000) * 127 / 4000 = 1500 * 127 / 4000 = 190500 / 4000 = 47.625
//   Integer division: 47
//   Expected: 47
//   Validation: assert_equal_uint8("-0.5g (-500mg)", 47, accel_to_midi_cc(-500));

// Test Case 8: Clamping - over-range positive
//   Input: 3000 milli-g (beyond ±2g sensor range)
//   Formula: (3000 + 2000) * 127 / 4000 = 5000 * 127 / 4000 = 635000 / 4000 = 158.75
//   Before clamping: 158
//   After clamping: 127 (max MIDI CC value)
//   Expected: 127
//   Validation: assert_equal_uint8("Over-range positive (3000mg)", 127, accel_to_midi_cc(3000));

/* -----------------------------------------------------------------------------
 * 2. MIDI MESSAGE CONSTRUCTION
 * 
 * MIDI CC message format (3 bytes):
 *   Byte 0 (Status):  0xB0 | (channel & 0x0F)    [Control Change on channel]
 *   Byte 1 (CC#):     cc_number & 0x7F            [Controller number 0-127]
 *   Byte 2 (Value):   value & 0x7F                [Controller value 0-127]
 * 
 * MIDI CC Status Bytes:
 *   0xB0 = Control Change on channel 0
 *   0xB1 = Control Change on channel 1
 *   ...
 *   0xBF = Control Change on channel 15
 * ----------------------------------------------------------------------------- */

// CALCULATION EXAMPLES:

// Test Case: Channel 0, CC 16 (X-axis), value 64
//   channel = 0, cc_number = 16 (0x10), value = 64 (0x40)
//   Byte 0: 0xB0 | (0 & 0x0F) = 0xB0 | 0x00 = 0xB0
//   Byte 1: 16 & 0x7F = 0x10 & 0x7F = 0x10
//   Byte 2: 64 & 0x7F = 0x40 & 0x7F = 0x40
//   Expected: [0xB0, 0x10, 0x40]
//   Validation: assert_midi_message("Channel 0, CC 16, value 64", 0xB0, 0x10, 0x40, msg);

// Test Case: Channel 1, CC 16, value 64
//   channel = 1, cc_number = 16, value = 64
//   Byte 0: 0xB0 | (1 & 0x0F) = 0xB0 | 0x01 = 0xB1
//   Byte 1: 16 & 0x7F = 0x10
//   Byte 2: 64 & 0x7F = 0x40
//   Expected: [0xB1, 0x10, 0x40]
//   Validation: assert_midi_message("Channel 1, CC 16, value 64", 0xB1, 0x10, 0x40, msg);

// Test Case: Channel 15 (max), CC 16, value 64
//   channel = 15, cc_number = 16, value = 64
//   Byte 0: 0xB0 | (15 & 0x0F) = 0xB0 | 0x0F = 0xBF
//   Byte 1: 16 & 0x7F = 0x10
//   Byte 2: 64 & 0x7F = 0x40
//   Expected: [0xBF, 0x10, 0x40]
//   Validation: assert_midi_message("Channel 15, CC 16, value 64", 0xBF, 0x10, 0x40, msg);

// Test Case: Masking test - invalid inputs
//   channel = 16 (out of range), cc_number = 128 (out of range), value = 200 (out of range)
//   Byte 0: 0xB0 | (16 & 0x0F) = 0xB0 | 0x00 = 0xB0  [wraps to channel 0]
//   Byte 1: 128 & 0x7F = 0x80 & 0x7F = 0x00          [wraps to CC 0]
//   Byte 2: 200 & 0x7F = 0xC8 & 0x7F = 0x48          [wraps to 72 decimal]
//   Expected: [0xB0, 0x00, 0x48]
//   Validation: assert_midi_message("Masking test (ch=16, cc=128, val=200)", 0xB0, 0x00, 0x48, msg);

/* -----------------------------------------------------------------------------
 * CC NUMBER DEFINITIONS (from main.c lines 54-56)
 * ----------------------------------------------------------------------------- */

// #define MIDI_CC_X_AXIS 16  --> 0x10 in hex
// #define MIDI_CC_Y_AXIS 17  --> 0x11 in hex
// #define MIDI_CC_Z_AXIS 18  --> 0x12 in hex

/* ============================================================================
 * VERIFICATION PROCESS
 * ============================================================================ */

/*
 * To verify any test case manually:
 * 
 * 1. For accel_to_midi_cc():
 *    a) Take input value (e.g., 500 milli-g)
 *    b) Apply formula: (500 + 2000) * 127 / 4000 = 79.375
 *    c) Integer division truncates: 79
 *    d) Check if clamping needed (0-127 range): No
 *    e) Expected result: 79
 * 
 * 2. For MIDI message construction:
 *    a) Identify channel, CC number, value
 *    b) Calculate status byte: 0xB0 | channel
 *    c) Mask CC number: cc & 0x7F
 *    d) Mask value: value & 0x7F
 *    e) Expected result: [status, cc, value]
 * 
 * 3. Run the test:
 *    cd /Users/richmaes/src/guitaracc/basestation/test
 *    make test
 * 
 * 4. If test fails:
 *    - Check formula implementation in main.c
 *    - Verify expected value calculation
 *    - Check for integer overflow or truncation issues
 */

/* ============================================================================
 * QUICK REFERENCE: Line Numbers in test_midi_cc.c
 * ============================================================================ */

/*
 * Validation Functions:
 *   - assert_equal_uint8():    Lines 76-87
 *   - assert_midi_message():   Lines 89-103
 * 
 * Test Cases:
 *   - test_accel_to_midi_conversion():      Lines 110-130
 *   - test_midi_message_construction():     Lines 132-154
 *   - test_complete_accel_to_midi_flow():   Lines 156-198
 * 
 * Functions Under Test (copied from main.c):
 *   - accel_to_midi_cc():           Lines 32-44
 *   - construct_midi_cc_msg():      Lines 46-56
 */

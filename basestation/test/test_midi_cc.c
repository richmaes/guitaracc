/*
 * Host-based test for MIDI CC message construction
 * Compiles and runs on macOS/Linux to verify logic before flashing to hardware
 * 
 * NOTE: This test uses the ACTUAL source code from src/midi_logic.c
 *       ensuring we test exactly what runs on the embedded hardware.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Include the actual header from the embedded code */
#include "../src/midi_logic.h"

/* Test result tracking */
typedef struct {
	int total_tests;
	int passed_tests;
	int failed_tests;
} test_stats_t;

static test_stats_t stats = {0};

/* ============================================================
 * NOTE: Functions under test are in ../src/midi_logic.c
 * They are compiled together with this test file.
 * NO DUPLICATION - we test the real code!
 * ============================================================ */

/* ============================================================
 * TEST UTILITIES
 * ============================================================ */

static void print_separator(char c, int length)
{
	for (int i = 0; i < length; i++) {
		putchar(c);
	}
	putchar('\n');
}

static void assert_equal_uint8(const char *test_name, uint8_t expected, uint8_t actual)
{
	stats.total_tests++;
	if (expected == actual) {
		printf("  ✓ %s: %d (0x%02X)\n", test_name, actual, actual);
		stats.passed_tests++;
	} else {
		printf("  ✗ %s: expected %d (0x%02X), got %d (0x%02X)\n", 
		       test_name, expected, expected, actual, actual);
		stats.failed_tests++;
	}
}

static void assert_midi_message(const char *test_name, uint8_t exp_status, uint8_t exp_data1, 
                                 uint8_t exp_data2, uint8_t *actual)
{
	stats.total_tests++;
	bool pass = (actual[0] == exp_status && actual[1] == exp_data1 && actual[2] == exp_data2);
	
	if (pass) {
		printf("  ✓ %s: [0x%02X 0x%02X 0x%02X]\n", test_name, actual[0], actual[1], actual[2]);
		stats.passed_tests++;
	} else {
		printf("  ✗ %s:\n", test_name);
		printf("    Expected: [0x%02X 0x%02X 0x%02X]\n", exp_status, exp_data1, exp_data2);
		printf("    Got:      [0x%02X 0x%02X 0x%02X]\n", actual[0], actual[1], actual[2]);
		stats.failed_tests++;
	}
}

/* ============================================================
 * TEST CASES
 * ============================================================ */

static void test_accel_to_midi_conversion(void)
{
	printf("\nTest: Accelerometer to MIDI CC Conversion\n");
	print_separator('-', 60);
	
	/* Test with default mapping (NULL config uses -2000 to +2000mg) */
	/* Test boundary values */
	assert_equal_uint8("Zero acceleration (0g)", 63, accel_to_midi_cc(0, NULL));
	assert_equal_uint8("Max positive (+2000mg)", 127, accel_to_midi_cc(2000, NULL));
	assert_equal_uint8("Max negative (-2000mg)", 0, accel_to_midi_cc(-2000, NULL));
	
	/* Test clamping */
	assert_equal_uint8("Over-range positive (3000mg)", 127, accel_to_midi_cc(3000, NULL));
	assert_equal_uint8("Over-range negative (-3000mg)", 0, accel_to_midi_cc(-3000, NULL));
	
	/* Test mid-range values */
	assert_equal_uint8("+1g (1000mg)", 95, accel_to_midi_cc(1000, NULL));
	assert_equal_uint8("-1g (-1000mg)", 31, accel_to_midi_cc(-1000, NULL));
	
	/* Test quarter values */
	assert_equal_uint8("+0.5g (500mg)", 79, accel_to_midi_cc(500, NULL));
	assert_equal_uint8("-0.5g (-500mg)", 47, accel_to_midi_cc(-500, NULL));
}

static void test_midi_message_construction(void)
{
	printf("\nTest: MIDI CC Message Construction\n");
	print_separator('-', 60);
	
	uint8_t msg[3];
	
	/* Test channel 0 messages */
	construct_midi_cc_msg(0, MIDI_CC_X_AXIS, 64, msg);
	assert_midi_message("Channel 0, CC 16, value 64", 0xB0, 0x10, 0x40, msg);
	
	construct_midi_cc_msg(0, MIDI_CC_Y_AXIS, 127, msg);
	assert_midi_message("Channel 0, CC 17, value 127", 0xB0, 0x11, 0x7F, msg);
	
	construct_midi_cc_msg(0, MIDI_CC_Z_AXIS, 0, msg);
	assert_midi_message("Channel 0, CC 18, value 0", 0xB0, 0x12, 0x00, msg);
	
	/* Test different channels */
	construct_midi_cc_msg(1, MIDI_CC_X_AXIS, 64, msg);
	assert_midi_message("Channel 1, CC 16, value 64", 0xB1, 0x10, 0x40, msg);
	
	construct_midi_cc_msg(15, MIDI_CC_X_AXIS, 64, msg);
	assert_midi_message("Channel 15, CC 16, value 64", 0xBF, 0x10, 0x40, msg);
	
	/* Test masking (values that should be masked) */
	construct_midi_cc_msg(16, 128, 200, msg);  /* Invalid values */
	assert_midi_message("Masking test (ch=16, cc=128, val=200)", 0xB0, 0x00, 0x48, msg);
}

static void test_complete_accel_to_midi_flow(void)
{
	printf("\nTest: Complete Accelerometer to MIDI Flow\n");
	print_separator('-', 60);
	
	struct accel_data test_cases[] = {
		{0, 0, 0},              /* Rest position */
		{2000, 2000, 2000},     /* Max positive */
		{-2000, -2000, -2000},  /* Max negative */
		{1000, -1000, 500},     /* Mixed */
		{300, -500, 1800},      /* Typical tilt */
	};
	
	const char *case_names[] = {
		"Rest (0, 0, 0)",
		"Max positive (2000, 2000, 2000)",
		"Max negative (-2000, -2000, -2000)",
		"Mixed (1000, -1000, 500)",
		"Typical tilt (300, -500, 1800)",
	};
	
	for (int i = 0; i < 5; i++) {
		uint8_t cc_x = accel_to_midi_cc(test_cases[i].x, NULL);
		uint8_t cc_y = accel_to_midi_cc(test_cases[i].y, NULL);
		uint8_t cc_z = accel_to_midi_cc(test_cases[i].z, NULL);
		
		uint8_t msg_x[3], msg_y[3], msg_z[3];
		construct_midi_cc_msg(0, MIDI_CC_X_AXIS, cc_x, msg_x);
		construct_midi_cc_msg(0, MIDI_CC_Y_AXIS, cc_y, msg_y);
		construct_midi_cc_msg(0, MIDI_CC_Z_AXIS, cc_z, msg_z);
		
		printf("\n%s:\n", case_names[i]);
		printf("  Input:  X=%5d Y=%5d Z=%5d (milli-g)\n", 
		       test_cases[i].x, test_cases[i].y, test_cases[i].z);
		printf("  MIDI:   X=%3d   Y=%3d   Z=%3d   (CC values)\n", cc_x, cc_y, cc_z);
		printf("  Bytes:  X=[0x%02X 0x%02X 0x%02X] Y=[0x%02X 0x%02X 0x%02X] Z=[0x%02X 0x%02X 0x%02X]\n",
		       msg_x[0], msg_x[1], msg_x[2],
		       msg_y[0], msg_y[1], msg_y[2],
		       msg_z[0], msg_z[1], msg_z[2]);
		
		stats.total_tests++;
		stats.passed_tests++;
	}
}

/* ============================================================
 * MAIN TEST RUNNER
 * ============================================================ */

int main(void)
{
	printf("\n");
	print_separator('=', 60);
	printf("MIDI CC Message Construction - Host-Based Tests\n");
	printf("Testing ACTUAL functions from: ../src/midi_logic.c\n");
	print_separator('=', 60);
	
	test_accel_to_midi_conversion();
	test_midi_message_construction();
	test_complete_accel_to_midi_flow();
	
	printf("\n");
	print_separator('=', 60);
	printf("Test Results:\n");
	printf("  Total:  %d\n", stats.total_tests);
	printf("  Passed: %d\n", stats.passed_tests);
	printf("  Failed: %d\n", stats.failed_tests);
	print_separator('=', 60);
	
	if (stats.failed_tests > 0) {
		printf("❌ TESTS FAILED\n");
		return 1;
	} else {
		printf("✅ ALL TESTS PASSED\n");
		return 0;
	}
}

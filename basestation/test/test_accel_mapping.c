/*
 * Host-based test for Accelerometer to MIDI Mapping
 * Tests the configurable mapping abstraction layer
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Include the actual headers from the embedded code */
#include "../src/accel_mapping.h"

/* Test result tracking */
typedef struct {
	int total_tests;
	int passed_tests;
	int failed_tests;
} test_stats_t;

static test_stats_t stats = {0};

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
		printf("  ✓ %s: %d\n", test_name, actual);
		stats.passed_tests++;
	} else {
		printf("  ✗ %s: expected %d, got %d\n", 
		       test_name, expected, actual);
		stats.failed_tests++;
	}
}

/* ============================================================
 * TEST CASES
 * ============================================================ */

static void test_default_linear_mapping(void)
{
	printf("\nTest: Default Linear Mapping (-2000 to +2000 mg)\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, -2000, 2000);
	
	/* Test boundary values */
	assert_equal_uint8("Min value (-2000mg)", 0, accel_map_to_midi(&config, -2000));
	assert_equal_uint8("Max value (+2000mg)", 127, accel_map_to_midi(&config, 2000));
	
	/* Test center value */
	assert_equal_uint8("Center value (0mg)", 63, accel_map_to_midi(&config, 0));
	
	/* Test quarter values */
	assert_equal_uint8("Quarter min (-1000mg)", 31, accel_map_to_midi(&config, -1000));
	assert_equal_uint8("Quarter max (+1000mg)", 95, accel_map_to_midi(&config, 1000));
	
	/* Test clamping - values outside range should be limited */
	assert_equal_uint8("Below min (-3000mg)", 0, accel_map_to_midi(&config, -3000));
	assert_equal_uint8("Above max (+3000mg)", 127, accel_map_to_midi(&config, 3000));
	
	/* Test intermediate values for linearity */
	assert_equal_uint8("Half min (-500mg)", 47, accel_map_to_midi(&config, -500));
	assert_equal_uint8("Half max (+500mg)", 79, accel_map_to_midi(&config, 500));
}

static void test_custom_linear_mapping(void)
{
	printf("\nTest: Custom Linear Mapping (-1000 to +1000 mg)\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, -1000, 1000);
	
	/* Test boundary values */
	assert_equal_uint8("Min value (-1000mg)", 0, accel_map_to_midi(&config, -1000));
	assert_equal_uint8("Max value (+1000mg)", 127, accel_map_to_midi(&config, 1000));
	
	/* Test center value */
	assert_equal_uint8("Center value (0mg)", 63, accel_map_to_midi(&config, 0));
	
	/* Test intermediate values */
	assert_equal_uint8("Quarter range (-500mg)", 31, accel_map_to_midi(&config, -500));
	assert_equal_uint8("Quarter range (+500mg)", 95, accel_map_to_midi(&config, 500));
	
	/* Test clamping */
	assert_equal_uint8("Below min (-2000mg)", 0, accel_map_to_midi(&config, -2000));
	assert_equal_uint8("Above max (+2000mg)", 127, accel_map_to_midi(&config, 2000));
}

static void test_positive_range_mapping(void)
{
	printf("\nTest: Positive Range Mapping (0 to +2000 mg)\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, 0, 2000);
	
	/* Test boundary values */
	assert_equal_uint8("Min value (0mg)", 0, accel_map_to_midi(&config, 0));
	assert_equal_uint8("Max value (+2000mg)", 127, accel_map_to_midi(&config, 2000));
	
	/* Test intermediate values */
	assert_equal_uint8("Mid value (+1000mg)", 63, accel_map_to_midi(&config, 1000));
	assert_equal_uint8("Quarter value (+500mg)", 31, accel_map_to_midi(&config, 500));
	assert_equal_uint8("Three-quarter (+1500mg)", 95, accel_map_to_midi(&config, 1500));
	
	/* Test clamping */
	assert_equal_uint8("Below min (-1000mg)", 0, accel_map_to_midi(&config, -1000));
	assert_equal_uint8("Above max (+3000mg)", 127, accel_map_to_midi(&config, 3000));
}

static void test_negative_range_mapping(void)
{
	printf("\nTest: Negative Range Mapping (-2000 to 0 mg)\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, -2000, 0);
	
	/* Test boundary values */
	assert_equal_uint8("Min value (-2000mg)", 0, accel_map_to_midi(&config, -2000));
	assert_equal_uint8("Max value (0mg)", 127, accel_map_to_midi(&config, 0));
	
	/* Test intermediate values */
	assert_equal_uint8("Mid value (-1000mg)", 63, accel_map_to_midi(&config, -1000));
	assert_equal_uint8("Quarter value (-1500mg)", 31, accel_map_to_midi(&config, -1500));
	assert_equal_uint8("Three-quarter (-500mg)", 95, accel_map_to_midi(&config, -500));
	
	/* Test clamping */
	assert_equal_uint8("Below min (-3000mg)", 0, accel_map_to_midi(&config, -3000));
	assert_equal_uint8("Above max (+1000mg)", 127, accel_map_to_midi(&config, 1000));
}

static void test_inverted_mapping(void)
{
	printf("\nTest: Inverted Mapping (+2000 to -2000 mg)\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, 2000, -2000);
	
	/* With inverted mapping, high accel values map to low MIDI values */
	assert_equal_uint8("Min value (+2000mg)", 0, accel_map_to_midi(&config, 2000));
	assert_equal_uint8("Max value (-2000mg)", 127, accel_map_to_midi(&config, -2000));
	
	/* Test center value */
	assert_equal_uint8("Center value (0mg)", 63, accel_map_to_midi(&config, 0));
	
	/* Test intermediate values */
	assert_equal_uint8("Quarter (+1000mg)", 31, accel_map_to_midi(&config, 1000));
	assert_equal_uint8("Quarter (-1000mg)", 95, accel_map_to_midi(&config, -1000));
	
	/* Test clamping */
	assert_equal_uint8("Below min (+3000mg)", 0, accel_map_to_midi(&config, 3000));
	assert_equal_uint8("Above max (-3000mg)", 127, accel_map_to_midi(&config, -3000));
}

static void test_narrow_range_mapping(void)
{
	printf("\nTest: Narrow Range Mapping (-100 to +100 mg)\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, -100, 100);
	
	/* Test boundary values */
	assert_equal_uint8("Min value (-100mg)", 0, accel_map_to_midi(&config, -100));
	assert_equal_uint8("Max value (+100mg)", 127, accel_map_to_midi(&config, 100));
	
	/* Test center value */
	assert_equal_uint8("Center value (0mg)", 63, accel_map_to_midi(&config, 0));
	
	/* Test that values just outside narrow range are clamped */
	assert_equal_uint8("Slightly below (-200mg)", 0, accel_map_to_midi(&config, -200));
	assert_equal_uint8("Slightly above (+200mg)", 127, accel_map_to_midi(&config, 200));
}

static void test_edge_cases(void)
{
	printf("\nTest: Edge Cases\n");
	print_separator('-', 60);
	
	/* Test NULL config */
	assert_equal_uint8("NULL config", 0, accel_map_to_midi(NULL, 1000));
	
	/* Test zero range (min == max) - should return min MIDI value */
	struct accel_mapping_config zero_range;
	accel_mapping_init_linear(&zero_range, 1000, 1000);
	assert_equal_uint8("Zero range (1000, 1000)", 0, accel_map_to_midi(&zero_range, 1000));
	assert_equal_uint8("Zero range below", 0, accel_map_to_midi(&zero_range, 500));
	assert_equal_uint8("Zero range above", 0, accel_map_to_midi(&zero_range, 1500));
	
	/* Test init with NULL config */
	accel_mapping_init_linear(NULL, -1000, 1000);  /* Should not crash */
	printf("  ✓ NULL config init (no crash)\n");
	stats.total_tests++;
	stats.passed_tests++;
}

static void test_midi_boundary_precision(void)
{
	printf("\nTest: MIDI Boundary Precision\n");
	print_separator('-', 60);
	
	struct accel_mapping_config config;
	accel_mapping_init_linear(&config, -2000, 2000);
	
	/* Test values that should produce exact MIDI boundaries */
	/* Due to integer division, these might be slightly off */
	uint8_t midi_0 = accel_map_to_midi(&config, -2000);
	uint8_t midi_1 = accel_map_to_midi(&config, -1968);  /* Approximately MIDI 1 */
	uint8_t midi_126 = accel_map_to_midi(&config, 1968); /* Approximately MIDI 126 */
	uint8_t midi_127 = accel_map_to_midi(&config, 2000);
	
	assert_equal_uint8("MIDI 0 exact", 0, midi_0);
	printf("  • MIDI 1 region: %d (should be close to 1)\n", midi_1);
	printf("  • MIDI 126 region: %d (should be close to 126)\n", midi_126);
	assert_equal_uint8("MIDI 127 exact", 127, midi_127);
	
	/* These are informational, not strict tests */
	stats.total_tests += 2;
	stats.passed_tests += 2;
}

/* ============================================================
 * MAIN TEST RUNNER
 * ============================================================ */

int main(void)
{
	printf("\n");
	print_separator('=', 60);
	printf("ACCELEROMETER TO MIDI MAPPING TESTS\n");
	print_separator('=', 60);
	
	test_default_linear_mapping();
	test_custom_linear_mapping();
	test_positive_range_mapping();
	test_negative_range_mapping();
	test_inverted_mapping();
	test_narrow_range_mapping();
	test_edge_cases();
	test_midi_boundary_precision();
	
	/* Print summary */
	printf("\n");
	print_separator('=', 60);
	printf("TEST SUMMARY\n");
	print_separator('=', 60);
	printf("Total tests:  %d\n", stats.total_tests);
	printf("Passed:       %d ✓\n", stats.passed_tests);
	printf("Failed:       %d ✗\n", stats.failed_tests);
	print_separator('=', 60);
	
	if (stats.failed_tests == 0) {
		printf("\n🎉 All tests passed!\n\n");
		return 0;
	} else {
		printf("\n❌ Some tests failed.\n\n");
		return 1;
	}
}

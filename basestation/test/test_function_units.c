/*
 * Function Units Unit Tests
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../src/function_units.h"

/* Test statistics */
static int total_tests = 0;
static int passed_tests = 0;
static int failed_tests = 0;

/* Test utilities */
static void print_separator(char c, int length)
{
	for (int i = 0; i < length; i++) {
		putchar(c);
	}
	putchar('\n');
}

static void assert_equal_int16(const char *test_name, int16_t expected, int16_t actual)
{
	total_tests++;
	if (expected == actual) {
		printf("  ✓ %s: %d\n", test_name, actual);
		passed_tests++;
	} else {
		printf("  ✗ %s: expected %d, got %d\n", test_name, expected, actual);
		failed_tests++;
	}
}

static void assert_true(const char *test_name, bool condition)
{
	total_tests++;
	if (condition) {
		printf("  ✓ %s\n", test_name);
		passed_tests++;
	} else {
		printf("  ✗ %s: assertion failed\n", test_name);
		failed_tests++;
	}
}

/* ============================================================
 * TEST CASES
 * ============================================================ */

static void test_func_passthrough(void)
{
	printf("\nTest: Passthrough Function\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init(&func, FUNC_PASSTHROUGH);
	
	assert_equal_int16("Input 0", 0, func_process(&func, 0));
	assert_equal_int16("Input 63", 63, func_process(&func, 63));
	assert_equal_int16("Input 127", 127, func_process(&func, 127));
	assert_equal_int16("Input -100", -100, func_process(&func, -100));
	assert_equal_int16("Input 200", 200, func_process(&func, 200));
}

static void test_func_linear_default(void)
{
	printf("\nTest: Linear Function - Default (±2g)\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init(&func, FUNC_LINEAR);
	
	/* Default is -2000 to +2000 → 0 to 127 */
	assert_equal_int16("Min (-2000mg)", 0, func_process(&func, -2000));
	assert_equal_int16("Center (0mg)", 63, func_process(&func, 0));
	assert_equal_int16("Max (+2000mg)", 127, func_process(&func, 2000));
	assert_equal_int16("Quarter (+1000mg)", 95, func_process(&func, 1000));
	assert_equal_int16("Below min (-3000mg)", 0, func_process(&func, -3000));
	assert_equal_int16("Above max (+3000mg)", 127, func_process(&func, 3000));
}

static void test_func_linear_custom(void)
{
	printf("\nTest: Linear Function - Custom Range\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init_linear(&func, -1000, 1000, 0, 127);
	
	/* ±1g to full MIDI range */
	assert_equal_int16("Min (-1000mg)", 0, func_process(&func, -1000));
	assert_equal_int16("Center (0mg)", 63, func_process(&func, 0));
	assert_equal_int16("Max (+1000mg)", 127, func_process(&func, 1000));
}

static void test_func_linear_positive_only(void)
{
	printf("\nTest: Linear Function - Positive Range Only\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init_linear(&func, 0, 2000, 0, 127);
	
	/* 0 to +2g → 0 to 127 */
	assert_equal_int16("Min (0mg)", 0, func_process(&func, 0));
	assert_equal_int16("Half (1000mg)", 63, func_process(&func, 1000));
	assert_equal_int16("Max (2000mg)", 127, func_process(&func, 2000));
}

static void test_func_linear_inverted(void)
{
	printf("\nTest: Linear Function - Inverted Range\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init_linear(&func, 2000, -2000, 0, 127);
	
	/* Inverted: +2g → 0, -2g → 127 */
	assert_equal_int16("Inverted max (+2000mg)", 0, func_process(&func, 2000));
	/* Note: integer rounding may give 63 or 64 - both are acceptable */
	int16_t center = func_process(&func, 0);
	assert_true("Inverted center (0mg) near midpoint", center == 63 || center == 64);
	assert_equal_int16("Inverted min (-2000mg)", 127, func_process(&func, -2000));
}

static void test_func_deadzone(void)
{
	printf("\nTest: Deadzone Function\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init_deadzone(&func, 100);
	
	/* Values below threshold should be zeroed */
	assert_equal_int16("Below threshold (50)", 0, func_process(&func, 50));
	assert_equal_int16("Below threshold (-50)", 0, func_process(&func, -50));
	assert_equal_int16("At threshold (100)", 100, func_process(&func, 100));
	assert_equal_int16("Above threshold (200)", 200, func_process(&func, 200));
	assert_equal_int16("Above threshold (-200)", -200, func_process(&func, -200));
}

static void test_func_invert(void)
{
	printf("\nTest: Invert Function\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init(&func, FUNC_INVERT);
	
	assert_equal_int16("Invert 0", 127, func_process(&func, 0));
	assert_equal_int16("Invert 63", 64, func_process(&func, 63));
	assert_equal_int16("Invert 127", 0, func_process(&func, 127));
	assert_equal_int16("Invert 64", 63, func_process(&func, 64));
}

static void test_func_scale(void)
{
	printf("\nTest: Scale Function\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init(&func, FUNC_SCALE);
	
	/* Default is 100 (1.0x, no change) */
	assert_equal_int16("Scale 1.0x (50)", 50, func_process(&func, 50));
	
	/* 1.5x scale */
	func.params[0] = 150;
	assert_equal_int16("Scale 1.5x (50)", 75, func_process(&func, 50));
	assert_equal_int16("Scale 1.5x (100, clamped)", 127, func_process(&func, 100));
	
	/* 0.5x scale */
	func.params[0] = 50;
	assert_equal_int16("Scale 0.5x (100)", 50, func_process(&func, 100));
}

static void test_func_clamp(void)
{
	printf("\nTest: Clamp Function\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init(&func, FUNC_CLAMP);
	func.params[0] = 20;   /* min */
	func.params[1] = 100;  /* max */
	
	assert_equal_int16("Within range (50)", 50, func_process(&func, 50));
	assert_equal_int16("Below min (10)", 20, func_process(&func, 10));
	assert_equal_int16("Above max (120)", 100, func_process(&func, 120));
	assert_equal_int16("At min (20)", 20, func_process(&func, 20));
	assert_equal_int16("At max (100)", 100, func_process(&func, 100));
}

static void test_func_validation(void)
{
	printf("\nTest: Function Validation\n");
	print_separator('-', 60);
	
	struct function_unit func;
	
	/* Valid passthrough */
	func_init(&func, FUNC_PASSTHROUGH);
	assert_true("Passthrough valid", func_validate(&func));
	
	/* Valid linear */
	func_init(&func, FUNC_LINEAR);
	assert_true("Linear valid", func_validate(&func));
	
	/* Valid deadzone */
	func_init_deadzone(&func, 100);
	assert_true("Deadzone valid", func_validate(&func));
	
	/* Disabled is always valid */
	func_init(&func, FUNC_DISABLED);
	assert_true("Disabled valid", func_validate(&func));
}

static void test_func_disabled(void)
{
	printf("\nTest: Disabled Function\n");
	print_separator('-', 60);
	
	struct function_unit func;
	func_init(&func, FUNC_DISABLED);
	
	/* Disabled function should return 0 */
	assert_equal_int16("Disabled (input 100)", 0, func_process(&func, 100));
	assert_equal_int16("Disabled (input 50)", 0, func_process(&func, 50));
}

static void test_func_get_name(void)
{
	printf("\nTest: Function Type Names\n");
	print_separator('-', 60);
	
	assert_true("Passthrough name", 
	            strcmp(func_get_name(FUNC_PASSTHROUGH), "Passthrough") == 0);
	assert_true("Linear name", 
	            strcmp(func_get_name(FUNC_LINEAR), "Linear") == 0);
	assert_true("Deadzone name", 
	            strcmp(func_get_name(FUNC_DEADZONE), "Deadzone") == 0);
	assert_true("Unknown type", 
	            strcmp(func_get_name(99), "Unknown") == 0);
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
	printf("\n");
	print_separator('=', 60);
	printf("FUNCTION UNITS UNIT TESTS\n");
	print_separator('=', 60);
	
	test_func_passthrough();
	test_func_linear_default();
	test_func_linear_custom();
	test_func_linear_positive_only();
	test_func_linear_inverted();
	test_func_deadzone();
	test_func_invert();
	test_func_scale();
	test_func_clamp();
	test_func_validation();
	test_func_disabled();
	test_func_get_name();
	
	printf("\n");
	print_separator('=', 60);
	printf("TEST SUMMARY\n");
	print_separator('=', 60);
	printf("Total tests:  %d\n", total_tests);
	printf("Passed:       %d\n", passed_tests);
	printf("Failed:       %d\n", failed_tests);
	printf("Success rate: %.1f%%\n", 
	       total_tests > 0 ? (100.0 * passed_tests / total_tests) : 0.0);
	print_separator('=', 60);
	
	return (failed_tests == 0) ? 0 : 1;
}

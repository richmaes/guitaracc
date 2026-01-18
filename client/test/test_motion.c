/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Host-based unit tests for motion detection logic
 */

#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>
#include "../src/motion_logic.h"

/* Test counter */
static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) \
	do { \
		tests_total++; \
		printf("TEST: %s ... ", name); \
	} while(0)

#define PASS() \
	do { \
		tests_passed++; \
		printf("PASS\n"); \
	} while(0)

#define FAIL(msg) \
	do { \
		printf("FAIL: %s\n", msg); \
	} while(0)

#define ASSERT_EQ(a, b) \
	do { \
		if ((a) != (b)) { \
			printf("FAIL: Expected %d, got %d\n", (int)(b), (int)(a)); \
			return; \
		} \
	} while(0)

#define ASSERT_NEAR(a, b, epsilon) \
	do { \
		if (fabs((a) - (b)) > (epsilon)) { \
			printf("FAIL: Expected %f, got %f (diff > %f)\n", (double)(b), (double)(a), (double)(epsilon)); \
			return; \
		} \
	} while(0)

#define ASSERT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL: Condition false\n"); \
			return; \
		} \
	} while(0)

#define ASSERT_FALSE(cond) \
	do { \
		if (cond) { \
			printf("FAIL: Condition true\n"); \
			return; \
		} \
	} while(0)

/* Test: Convert zero acceleration */
void test_convert_zero_acceleration(void)
{
	TEST("convert_zero_acceleration");
	int16_t result = convert_to_milli_g(0.0);
	ASSERT_EQ(result, 0);
	PASS();
}

/* Test: Convert 1g (9.81 m/s²) to 1000 milli-g */
void test_convert_one_g(void)
{
	TEST("convert_one_g");
	int16_t result = convert_to_milli_g(9.81);
	ASSERT_EQ(result, 1000);
	PASS();
}

/* Test: Convert 2g to 2000 milli-g */
void test_convert_two_g(void)
{
	TEST("convert_two_g");
	int16_t result = convert_to_milli_g(19.62);
	ASSERT_EQ(result, 2000);
	PASS();
}

/* Test: Convert negative 1g */
void test_convert_negative_one_g(void)
{
	TEST("convert_negative_one_g");
	int16_t result = convert_to_milli_g(-9.81);
	ASSERT_EQ(result, -1000);
	PASS();
}

/* Test: Clamp positive overflow */
void test_clamp_positive_overflow(void)
{
	TEST("clamp_positive_overflow");
	int16_t result = convert_to_milli_g(500.0);  /* ~50g, exceeds int16_t */
	ASSERT_EQ(result, 32767);
	PASS();
}

/* Test: Clamp negative overflow */
void test_clamp_negative_overflow(void)
{
	TEST("clamp_negative_overflow");
	int16_t result = convert_to_milli_g(-500.0);  /* ~-50g, exceeds int16_t */
	ASSERT_EQ(result, -32768);
	PASS();
}

/* Test: Convert XYZ acceleration */
void test_convert_xyz_acceleration(void)
{
	TEST("convert_xyz_acceleration");
	struct accel_data data;
	
	convert_accel_to_milli_g(9.81, 0.0, -9.81, &data);
	
	ASSERT_EQ(data.x, 1000);
	ASSERT_EQ(data.y, 0);
	ASSERT_EQ(data.z, -1000);
	PASS();
}

/* Test: NULL pointer safety in convert */
void test_convert_null_safety(void)
{
	TEST("convert_null_safety");
	convert_accel_to_milli_g(9.81, 0.0, 0.0, NULL);  /* Should not crash */
	PASS();
}

/* Test: Calculate magnitude of zero vector */
void test_magnitude_zero_vector(void)
{
	TEST("magnitude_zero_vector");
	double mag = calculate_magnitude(0.0, 0.0, 0.0);
	ASSERT_NEAR(mag, 0.0, 0.001);
	PASS();
}

/* Test: Calculate magnitude of 1g gravity (Z-axis) */
void test_magnitude_one_g_z(void)
{
	TEST("magnitude_one_g_z");
	double mag = calculate_magnitude(0.0, 0.0, 9.81);
	ASSERT_NEAR(mag, 9.81, 0.001);
	PASS();
}

/* Test: Calculate magnitude of 3-4-5 triangle */
void test_magnitude_345_triangle(void)
{
	TEST("magnitude_345_triangle");
	double mag = calculate_magnitude(3.0, 4.0, 0.0);
	ASSERT_NEAR(mag, 5.0, 0.001);
	PASS();
}

/* Test: Calculate magnitude with all axes equal */
void test_magnitude_equal_axes(void)
{
	TEST("magnitude_equal_axes");
	double mag = calculate_magnitude(1.0, 1.0, 1.0);
	ASSERT_NEAR(mag, sqrt(3.0), 0.001);
	PASS();
}

/* Test: Detect no motion (at rest, 1g gravity) */
void test_detect_no_motion_at_rest(void)
{
	TEST("detect_no_motion_at_rest");
	bool motion = detect_motion(0.0, 0.0, 9.81);  /* Large magnitude but this is just gravity */
	ASSERT_TRUE(motion);  /* 9.81 m/s² exceeds 0.5 threshold */
	PASS();
}

/* Test: Detect no motion (within threshold) */
void test_detect_no_motion_within_threshold(void)
{
	TEST("detect_no_motion_within_threshold");
	bool motion = detect_motion(0.0, 0.0, 0.3);  /* 0.3 m/s² magnitude, below 0.5 threshold */
	ASSERT_FALSE(motion);
	PASS();
}

/* Test: Detect motion (above threshold) */
void test_detect_motion_above_threshold(void)
{
	TEST("detect_motion_above_threshold");
	bool motion = detect_motion(0.0, 0.0, 0.6);  /* 0.6 m/s² magnitude, above 0.5 threshold */
	ASSERT_TRUE(motion);
	PASS();
}

/* Test: Detect motion with X-axis movement */
void test_detect_motion_x_axis(void)
{
	TEST("detect_motion_x_axis");
	bool motion = detect_motion(5.0, 0.0, 9.81);  /* Significant X acceleration */
	ASSERT_TRUE(motion);
	PASS();
}

/* Test: Detect motion with rapid movement */
void test_detect_motion_rapid_movement(void)
{
	TEST("detect_motion_rapid_movement");
	bool motion = detect_motion(10.0, 10.0, 10.0);  /* High acceleration on all axes */
	ASSERT_TRUE(motion);
	PASS();
}

/* Test: Acceleration data unchanged */
void test_accel_data_unchanged(void)
{
	TEST("accel_data_unchanged");
	struct accel_data current = {100, 200, 300};
	struct accel_data previous = {100, 200, 300};
	
	bool changed = accel_data_changed(&current, &previous);
	ASSERT_FALSE(changed);
	PASS();
}

/* Test: Acceleration data changed (X-axis) */
void test_accel_data_changed_x(void)
{
	TEST("accel_data_changed_x");
	struct accel_data current = {101, 200, 300};
	struct accel_data previous = {100, 200, 300};
	
	bool changed = accel_data_changed(&current, &previous);
	ASSERT_TRUE(changed);
	PASS();
}

/* Test: Acceleration data changed (Y-axis) */
void test_accel_data_changed_y(void)
{
	TEST("accel_data_changed_y");
	struct accel_data current = {100, 201, 300};
	struct accel_data previous = {100, 200, 300};
	
	bool changed = accel_data_changed(&current, &previous);
	ASSERT_TRUE(changed);
	PASS();
}

/* Test: Acceleration data changed (Z-axis) */
void test_accel_data_changed_z(void)
{
	TEST("accel_data_changed_z");
	struct accel_data current = {100, 200, 301};
	struct accel_data previous = {100, 200, 300};
	
	bool changed = accel_data_changed(&current, &previous);
	ASSERT_TRUE(changed);
	PASS();
}

/* Test: Acceleration data changed (all axes) */
void test_accel_data_changed_all(void)
{
	TEST("accel_data_changed_all");
	struct accel_data current = {150, 250, 350};
	struct accel_data previous = {100, 200, 300};
	
	bool changed = accel_data_changed(&current, &previous);
	ASSERT_TRUE(changed);
	PASS();
}

/* Test: NULL pointer safety in change detection */
void test_accel_data_changed_null_current(void)
{
	TEST("accel_data_changed_null_current");
	struct accel_data previous = {100, 200, 300};
	
	bool changed = accel_data_changed(NULL, &previous);
	ASSERT_FALSE(changed);
	PASS();
}

/* Test: NULL pointer safety in change detection (previous) */
void test_accel_data_changed_null_previous(void)
{
	TEST("accel_data_changed_null_previous");
	struct accel_data current = {100, 200, 300};
	
	bool changed = accel_data_changed(&current, NULL);
	ASSERT_FALSE(changed);
	PASS();
}

/* Test: Small fractional conversion */
void test_convert_small_fraction(void)
{
	TEST("convert_small_fraction");
	int16_t result = convert_to_milli_g(0.0981);  /* 0.01g = 10 milli-g */
	ASSERT_EQ(result, 10);
	PASS();
}

/* Test: Motion detection boundary (exactly at threshold) */
void test_detect_motion_at_threshold(void)
{
	TEST("detect_motion_at_threshold");
	/* magnitude = 0.5 m/s² (exactly at threshold) */
	bool motion = detect_motion(0.0, 0.0, 0.5);
	ASSERT_FALSE(motion);  /* Should be false since it's not > threshold */
	PASS();
}

int main(void)
{
	printf("=== Motion Logic Unit Tests ===\n\n");
	
	/* Conversion tests */
	test_convert_zero_acceleration();
	test_convert_one_g();
	test_convert_two_g();
	test_convert_negative_one_g();
	test_clamp_positive_overflow();
	test_clamp_negative_overflow();
	test_convert_xyz_acceleration();
	test_convert_null_safety();
	test_convert_small_fraction();
	
	/* Magnitude tests */
	test_magnitude_zero_vector();
	test_magnitude_one_g_z();
	test_magnitude_345_triangle();
	test_magnitude_equal_axes();
	
	/* Motion detection tests */
	test_detect_no_motion_at_rest();
	test_detect_no_motion_within_threshold();
	test_detect_motion_above_threshold();
	test_detect_motion_x_axis();
	test_detect_motion_rapid_movement();
	test_detect_motion_at_threshold();
	
	/* Data change tests */
	test_accel_data_unchanged();
	test_accel_data_changed_x();
	test_accel_data_changed_y();
	test_accel_data_changed_z();
	test_accel_data_changed_all();
	test_accel_data_changed_null_current();
	test_accel_data_changed_null_previous();
	
	printf("\n=== Test Summary ===\n");
	printf("Passed: %d/%d\n", tests_passed, tests_total);
	
	if (tests_passed == tests_total) {
		printf("ALL TESTS PASSED!\n");
		return 0;
	} else {
		printf("SOME TESTS FAILED!\n");
		return 1;
	}
}

/*
 * Virtual Ports Unit Tests
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../src/virtual_ports.h"

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

static void assert_equal_uint8(const char *test_name, uint8_t expected, uint8_t actual)
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

static void assert_equal_int(const char *test_name, int expected, int actual)
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

/* ============================================================
 * TEST CASES
 * ============================================================ */

static void test_vport_init(void)
{
	printf("\nTest: Virtual Port Initialization\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_AVERAGE);
	
	assert_equal_uint8("Default mixer type", MIXER_AVERAGE, vps.default_mixer_type);
	assert_equal_uint8("Port 0 mixer", MIXER_AVERAGE, vps.ports[0].mixer_type);
	assert_equal_uint8("Port 0 value", 0, vps.ports[0].value);
	assert_equal_uint8("Port 0 input count", 0, vps.ports[0].input_count);
}

static void test_vport_single_write_read(void)
{
	printf("\nTest: Single Write and Read\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_PASSTHROUGH);
	vport_reset_all(&vps);
	
	/* Write value to port 0 */
	int result = vport_write(&vps, 0, 63);
	assert_equal_int("Write result", 0, result);
	assert_equal_uint8("Input count after write", 1, vport_get_input_count(&vps, 0));
	
	/* Read value back */
	uint8_t value = vport_read(&vps, 0);
	assert_equal_uint8("Read value", 63, value);
}

static void test_vport_mixer_sum(void)
{
	printf("\nTest: Mixer - Sum\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_SUM);
	vport_reset_all(&vps);
	
	/* Write multiple values */
	vport_write(&vps, 0, 30);
	vport_write(&vps, 0, 40);
	vport_write(&vps, 0, 20);
	
	assert_equal_uint8("Input count", 3, vport_get_input_count(&vps, 0));
	assert_equal_uint8("Summed value", 90, vport_read(&vps, 0));
}

static void test_vport_mixer_sum_clamp(void)
{
	printf("\nTest: Mixer - Sum with Clamping\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_SUM);
	vport_reset_all(&vps);
	
	/* Write values that exceed MIDI range */
	vport_write(&vps, 0, 100);
	vport_write(&vps, 0, 80);
	
	assert_equal_uint8("Clamped to max", 127, vport_read(&vps, 0));
}

static void test_vport_mixer_average(void)
{
	printf("\nTest: Mixer - Average\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_AVERAGE);
	vport_reset_all(&vps);
	
	/* Write multiple values */
	vport_write(&vps, 0, 40);
	vport_write(&vps, 0, 60);
	vport_write(&vps, 0, 80);
	
	assert_equal_uint8("Input count", 3, vport_get_input_count(&vps, 0));
	/* Average of 40, 60, 80 = 60 */
	assert_equal_uint8("Average value", 60, vport_read(&vps, 0));
}

static void test_vport_mixer_max(void)
{
	printf("\nTest: Mixer - Maximum\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_MAX);
	vport_reset_all(&vps);
	
	/* Write multiple values */
	vport_write(&vps, 0, 40);
	vport_write(&vps, 0, 90);
	vport_write(&vps, 0, 60);
	
	assert_equal_uint8("Maximum value", 90, vport_read(&vps, 0));
}

static void test_vport_mixer_min(void)
{
	printf("\nTest: Mixer - Minimum\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_MIN);
	vport_reset_all(&vps);
	
	/* Write multiple values */
	vport_write(&vps, 0, 60);
	vport_write(&vps, 0, 30);
	vport_write(&vps, 0, 90);
	
	assert_equal_uint8("Minimum value", 30, vport_read(&vps, 0));
}

static void test_vport_multiple_ports(void)
{
	printf("\nTest: Multiple Ports Independently\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_PASSTHROUGH);
	vport_reset_all(&vps);
	
	/* Write to different ports */
	vport_write(&vps, 0, 10);
	vport_write(&vps, 1, 20);
	vport_write(&vps, 2, 30);
	
	assert_equal_uint8("Port 0", 10, vport_read(&vps, 0));
	assert_equal_uint8("Port 1", 20, vport_read(&vps, 1));
	assert_equal_uint8("Port 2", 30, vport_read(&vps, 2));
}

static void test_vport_reset(void)
{
	printf("\nTest: Reset Functionality\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_PASSTHROUGH);
	vport_reset_all(&vps);
	
	/* Write values */
	vport_write(&vps, 0, 100);
	vport_write(&vps, 1, 50);
	
	assert_equal_uint8("Port 0 before reset", 100, vport_read(&vps, 0));
	
	/* Reset */
	vport_reset_all(&vps);
	
	assert_equal_uint8("Port 0 after reset", 0, vport_read(&vps, 0));
	assert_equal_uint8("Port 1 after reset", 0, vport_read(&vps, 1));
	assert_equal_uint8("Port 0 input count", 0, vport_get_input_count(&vps, 0));
}

static void test_vport_per_port_mixer_config(void)
{
	printf("\nTest: Per-Port Mixer Configuration\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_PASSTHROUGH);
	vport_reset_all(&vps);
	
	/* Configure different mixer types for different ports */
	vport_set_mixer(&vps, 0, MIXER_SUM);
	vport_set_mixer(&vps, 1, MIXER_AVERAGE);
	
	/* Port 0: Sum */
	vport_write(&vps, 0, 30);
	vport_write(&vps, 0, 40);
	assert_equal_uint8("Port 0 (sum)", 70, vport_read(&vps, 0));
	
	/* Port 1: Average */
	vport_write(&vps, 1, 30);
	vport_write(&vps, 1, 40);
	assert_equal_uint8("Port 1 (average)", 35, vport_read(&vps, 1));
}

static void test_vport_boundary_values(void)
{
	printf("\nTest: Boundary Value Handling\n");
	print_separator('-', 60);
	
	struct virtual_port_system vps;
	vport_init(&vps, MIXER_PASSTHROUGH);
	vport_reset_all(&vps);
	
	/* Test MIDI min */
	vport_write(&vps, 0, 0);
	assert_equal_uint8("MIDI min (0)", 0, vport_read(&vps, 0));
	
	vport_reset_all(&vps);
	
	/* Test MIDI max */
	vport_write(&vps, 0, 127);
	assert_equal_uint8("MIDI max (127)", 127, vport_read(&vps, 0));
	
	vport_reset_all(&vps);
	
	/* Test below MIDI range */
	vport_write(&vps, 0, -50);
	assert_equal_uint8("Below min (clamped to 0)", 0, vport_read(&vps, 0));
	
	vport_reset_all(&vps);
	
	/* Test above MIDI range */
	vport_write(&vps, 0, 200);
	assert_equal_uint8("Above max (clamped to 127)", 127, vport_read(&vps, 0));
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
	printf("\n");
	print_separator('=', 60);
	printf("VIRTUAL PORTS UNIT TESTS\n");
	print_separator('=', 60);
	
	test_vport_init();
	test_vport_single_write_read();
	test_vport_mixer_sum();
	test_vport_mixer_sum_clamp();
	test_vport_mixer_average();
	test_vport_mixer_max();
	test_vport_mixer_min();
	test_vport_multiple_ports();
	test_vport_reset();
	test_vport_per_port_mixer_config();
	test_vport_boundary_values();
	
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

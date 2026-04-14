/*
 * Topology Processor Integration Tests
 * Tests all 4 topology types end-to-end
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "../src/topology_processor.h"

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

static void test_topology_t1_simple_linear(void)
{
	printf("\nTest: Topology T1 - Simple Linear Processing\n");
	print_separator('-', 60);
	
	/* Setup configuration */
	struct patch_topology_config config;
	memset(&config, 0, sizeof(config));
	config.default_mixer_type = MIXER_PASSTHROUGH;
	
	/* Configure T1: Accel-X → Func-0 → MIDI CC 16 */
	struct topology_instance *t1 = &config.topologies[0];
	topology_init_default(t1, TOPO_T1);
	t1->accel_inputs[0] = 0;    /* X-axis */
	t1->func_units[0] = 0;      /* Function 0 */
	t1->midi_outputs[0] = 16;   /* CC 16 */
	t1->enabled = 1;
	
	/* Initialize processor */
	struct topology_processor proc;
	topo_proc_init(&proc, &config);
	
	/* Configure function 0 as linear: -2000 to +2000 → 0 to 127 */
	struct function_unit func;
	func_init_linear(&func, -2000, 2000, 0, 127);
	topo_proc_set_function(&proc, 0, &func);
	
	/* Test with accelerometer values */
	int16_t accel_data[6] = {0, 0, 0, 0, 0, 0};
	
	/* Test 1: Center value (0mg) */
	accel_data[0] = 0;
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	assert_equal_uint8("T1: Center (0mg → CC 63)", 63, topo_proc_get_midi_output(&proc, 0));
	
	/* Test 2: Min value (-2000mg) */
	accel_data[0] = -2000;
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	assert_equal_uint8("T1: Min (-2000mg → CC 0)", 0, topo_proc_get_midi_output(&proc, 0));
	
	/* Test 3: Max value (+2000mg) */
	accel_data[0] = 2000;
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	assert_equal_uint8("T1: Max (+2000mg → CC 127)", 127, topo_proc_get_midi_output(&proc, 0));
	
	/* Test 4: Intermediate value (+1000mg) */
	accel_data[0] = 1000;
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	assert_equal_uint8("T1: Half (+1000mg → CC 95)", 95, topo_proc_get_midi_output(&proc, 0));
}

static void test_topology_t2_two_inputs_merged(void)
{
	printf("\nTest: Topology T2 - Two Inputs Merged\n");
	print_separator('-', 60);
	
	/* Setup configuration */
	struct patch_topology_config config;
	memset(&config, 0, sizeof(config));
	config.default_mixer_type = MIXER_AVERAGE;
	
	/* Configure T2: Accel-X + Accel-Y → Func-0 → MIDI CC 16 */
	struct topology_instance *t2 = &config.topologies[0];
	topology_init_default(t2, TOPO_T2);
	t2->accel_inputs[0] = 0;    /* X-axis */
	t2->accel_inputs[1] = 1;    /* Y-axis */
	t2->func_units[0] = 0;      /* Function 0 */
	t2->midi_outputs[0] = 16;   /* CC 16 */
	t2->enabled = 1;
	
	/* Initialize processor */
	struct topology_processor proc;
	topo_proc_init(&proc, &config);
	
	/* Configure function 0 as passthrough (to test mixing) */
	struct function_unit func;
	func_init(&func, FUNC_PASSTHROUGH);
	topo_proc_set_function(&proc, 0, &func);
	
	/* Test with accelerometer values */
	int16_t accel_data[6] = {0, 0, 0, 0, 0, 0};
	
	/* Test: Average of X=40 and Y=60 should be 50 */
	accel_data[0] = 40;
	accel_data[1] = 60;
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	assert_equal_uint8("T2: Average (40+60)/2 = 50", 50, topo_proc_get_midi_output(&proc, 0));
	
	/* Test: Average of X=80 and Y=120 should be 100 */
	accel_data[0] = 80;
	accel_data[1] = 120;
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	assert_equal_uint8("T2: Average (80+120)/2 = 100", 100, topo_proc_get_midi_output(&proc, 0));
}

static void test_topology_t3_fan_out(void)
{
	printf("\nTest: Topology T3 - Fan-out to Multiple Outputs\n");
	print_separator('-', 60);
	
	/* Setup configuration */
	struct patch_topology_config config;
	memset(&config, 0, sizeof(config));
	config.default_mixer_type = MIXER_PASSTHROUGH;
	
	/* Configure T3: Accel-X → Func-0 → (MIDI CC 16, MIDI CC 17) */
	struct topology_instance *t3 = &config.topologies[0];
	topology_init_default(t3, TOPO_T3);
	t3->accel_inputs[0] = 0;    /* X-axis */
	t3->func_units[0] = 0;      /* Function 0 */
	t3->midi_outputs[0] = 16;   /* CC 16 */
	t3->midi_outputs[1] = 17;   /* CC 17 */
	t3->enabled = 1;
	
	/* Initialize processor */
	struct topology_processor proc;
	topo_proc_init(&proc, &config);
	
	/* Configure function 0 to output constant 75 for testing */
	struct function_unit func;
	func_init_linear(&func, -2000, 2000, 0, 127);
	topo_proc_set_function(&proc, 0, &func);
	
	/* Test with accelerometer value */
	int16_t accel_data[6] = {1000, 0, 0, 0, 0, 0};  /* +1000mg on X */
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	
	/* Both outputs should have the same value (fan-out) */
	uint8_t output0 = topo_proc_get_midi_output(&proc, 0);  /* CC 16 → index 0 */
	uint8_t output1 = topo_proc_get_midi_output(&proc, 1);  /* CC 17 → index 1 */
	
	assert_equal_uint8("T3: Output 1 (CC 16)", 95, output0);
	assert_equal_uint8("T3: Output 2 (CC 17)", 95, output1);
	assert_true("T3: Both outputs equal", output0 == output1);
}

static void test_topology_t4_cascaded(void)
{
	printf("\nTest: Topology T4 - Cascaded Processing\n");
	print_separator('-', 60);
	
	/* Setup configuration */
	struct patch_topology_config config;
	memset(&config, 0, sizeof(config));
	config.default_mixer_type = MIXER_AVERAGE;
	
	/* Configure T4: (Accel-X + Accel-Y) → Func-0 → MIDI CC 16 */
	/*                                           → Func-1 → MIDI CC 17 */
	struct topology_instance *t4 = &config.topologies[0];
	topology_init_default(t4, TOPO_T4);
	t4->accel_inputs[0] = 0;    /* X-axis */
	t4->accel_inputs[1] = 1;    /* Y-axis */
	t4->func_units[0] = 0;      /* Function 0 */
	t4->func_units[1] = 1;      /* Function 1 */
	t4->midi_outputs[0] = 16;   /* CC 16 */
	t4->midi_outputs[1] = 17;   /* CC 17 */
	t4->enabled = 1;
	
	/* Initialize processor */
	struct topology_processor proc;
	topo_proc_init(&proc, &config);
	
	/* Configure function 0 as passthrough */
	struct function_unit func0;
	func_init(&func0, FUNC_PASSTHROUGH);
	topo_proc_set_function(&proc, 0, &func0);
	
	/* Configure function 1 to scale by 1.5x */
	struct function_unit func1;
	func_init(&func1, FUNC_SCALE);
	func1.params[0] = 150;  /* 1.5x scale */
	topo_proc_set_function(&proc, 1, &func1);
	
	/* Test with X=40, Y=60 (average = 50) */
	int16_t accel_data[6] = {40, 60, 0, 0, 0, 0};
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	
	/* Output 1: passthrough of average (50) */
	assert_equal_uint8("T4: Output 1 (passthrough avg)", 50, 
	                   topo_proc_get_midi_output(&proc, 0));
	
	/* Output 2: scaled by 1.5x (50 * 1.5 = 75) */
	assert_equal_uint8("T4: Output 2 (scaled 1.5x)", 75, 
	                   topo_proc_get_midi_output(&proc, 1));
}

static void test_multiple_topologies_parallel(void)
{
	printf("\nTest: Multiple Topologies Running in Parallel\n");
	print_separator('-', 60);
	
	/* Setup configuration with 3 T1 instances for X, Y, Z axes */
	struct patch_topology_config config;
	memset(&config, 0, sizeof(config));
	config.default_mixer_type = MIXER_PASSTHROUGH;
	
	/* T1 instance 0: X-axis → Func-0 → CC 16 */
	config.topologies[0].topology_type = TOPO_T1;
	config.topologies[0].accel_inputs[0] = 0;
	config.topologies[0].func_units[0] = 0;
	config.topologies[0].midi_outputs[0] = 16;
	config.topologies[0].enabled = 1;
	
	/* T1 instance 1: Y-axis → Func-1 → CC 17 */
	config.topologies[1].topology_type = TOPO_T1;
	config.topologies[1].accel_inputs[0] = 1;
	config.topologies[1].func_units[0] = 1;
	config.topologies[1].midi_outputs[0] = 17;
	config.topologies[1].enabled = 1;
	
	/* T1 instance 2: Z-axis → Func-2 → CC 18 */
	config.topologies[2].topology_type = TOPO_T1;
	config.topologies[2].accel_inputs[0] = 2;
	config.topologies[2].func_units[0] = 2;
	config.topologies[2].midi_outputs[0] = 18;
	config.topologies[2].enabled = 1;
	
	/* Initialize processor */
	struct topology_processor proc;
	topo_proc_init(&proc, &config);
	
	/* Configure all functions as linear -2000 to +2000 */
	struct function_unit func;
	func_init_linear(&func, -2000, 2000, 0, 127);
	for (int i = 0; i < 3; i++) {
		topo_proc_set_function(&proc, i, &func);
	}
	
	/* Set distinct accelerometer values */
	int16_t accel_data[6] = {-2000, 0, 2000, 0, 0, 0};  /* X=min, Y=center, Z=max */
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	
	/* Verify each output independently */
	assert_equal_uint8("Parallel: X-axis @ CC16", 0, topo_proc_get_midi_output(&proc, 0));
	assert_equal_uint8("Parallel: Y-axis @ CC17", 63, topo_proc_get_midi_output(&proc, 1));
	assert_equal_uint8("Parallel: Z-axis @ CC18", 127, topo_proc_get_midi_output(&proc, 2));
}

static void test_default_patch_config(void)
{
	printf("\nTest: Default Patch Configuration\n");
	print_separator('-', 60);
	
	/* Initialize with defaults */
	struct patch_topology_config config;
	topology_patch_init_default(&config);
	
	/* Should configure all 6 axes with T1 */
	for (int i = 0; i < 6; i++) {
		assert_true("Default axis enabled", config.topologies[i].enabled == 1);
		assert_true("Default is T1", config.topologies[i].topology_type == TOPO_T1);
		assert_true("Default accel mapping", config.topologies[i].accel_inputs[0] == i);
	}
	
	/* Test processing with defaults */
	struct topology_processor proc;
	topo_proc_init(&proc, &config);
	
	/* Set all function units to linear */
	struct function_unit func;
	func_init_linear(&func, -2000, 2000, 0, 127);
	for (int i = 0; i < 6; i++) {
		topo_proc_set_function(&proc, i, &func);
	}
	
	/* All axes at center (0) */
	int16_t accel_data[6] = {0, 0, 0, 0, 0, 0};
	topo_proc_set_accel_inputs(&proc, accel_data);
	topo_proc_execute(&proc);
	
	/* All outputs should be center (63) */
	for (int i = 0; i < 6; i++) {
		char test_name[64];
		snprintf(test_name, sizeof(test_name), "Default axis %d = 63", i);
		assert_equal_uint8(test_name, 63, topo_proc_get_midi_output(&proc, i));
	}
}

static void test_topology_validation(void)
{
	printf("\nTest: Topology Validation\n");
	print_separator('-', 60);
	
	struct topology_instance topo;
	
	/* Valid T1 */
	topology_init_default(&topo, TOPO_T1);
	assert_true("Valid T1", topology_validate(&topo));
	
	/* Invalid accel input */
	topo.accel_inputs[0] = 99;  /* Out of range */
	assert_true("Invalid accel input", !topology_validate(&topo));
	
	/* Reset and test disabled */
	topology_init_default(&topo, TOPO_DISABLED);
	assert_true("Disabled topology valid", topology_validate(&topo));
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
	printf("\n");
	print_separator('=', 60);
	printf("TOPOLOGY PROCESSOR INTEGRATION TESTS\n");
	print_separator('=', 60);
	
	test_topology_t1_simple_linear();
	test_topology_t2_two_inputs_merged();
	test_topology_t3_fan_out();
	test_topology_t4_cascaded();
	test_multiple_topologies_parallel();
	test_default_patch_config();
	test_topology_validation();
	
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

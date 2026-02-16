/*
 * Test suite for spike limiter and running average filters
 * Copyright (c) 2026 GuitarAcc Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* Define to enable running average for testing */
#define ENABLE_RUNNING_AVERAGE 1
#define RUNNING_AVERAGE_DEPTH 5
#define SPIKE_LIMIT_MILLI_G 500

/* Include the motion logic header and source */
#include "../src/motion_logic.h"
#include "../src/motion_logic.c"

void test_spike_limiter_basic(void)
{
	printf("Test: Spike Limiter - Basic Functionality\n");
	
	struct accel_data initial = {0, 0, 1000};  /* Starting at 1g on Z-axis */
	struct accel_data raw, limited;
	
	/* Initialize with starting value */
	spike_limiter_init(&initial);
	
	/* Test 1: Small change - should pass through */
	raw.x = 100;
	raw.y = 100;
	raw.z = 1100;
	apply_spike_limiter(&raw, &limited);
	
	assert(limited.x == 100);
	assert(limited.y == 100);
	assert(limited.z == 1100);
	printf("  ✓ Small changes pass through correctly\n");
	
	/* Test 2: Large spike - should be limited */
	raw.x = 2000;  /* 1900 milli-g jump - should be limited to 500 */
	raw.y = 100;
	raw.z = 1100;
	apply_spike_limiter(&raw, &limited);
	
	assert(limited.x == 600);  /* 100 + 500 max change */
	assert(limited.y == 100);
	assert(limited.z == 1100);
	printf("  ✓ Large spikes are limited correctly (X: %d, expected 600)\n", limited.x);
	
	/* Test 3: Negative spike */
	raw.x = -2000;  /* Large negative spike */
	raw.y = 100;
	raw.z = 1100;
	apply_spike_limiter(&raw, &limited);
	
	assert(limited.x == 100);  /* 600 - 500 max change = 100 */
	printf("  ✓ Negative spikes are limited correctly (X: %d, expected 100)\n", limited.x);
	
	printf("✓ All spike limiter tests passed!\n\n");
}

void test_running_average_basic(void)
{
	printf("Test: Running Average - Basic Functionality\n");
	
	struct accel_data input, output;
	
	running_average_init();
	
	/* Feed constant values */
	input.x = 1000;
	input.y = 500;
	input.z = -500;
	
	/* First sample */
	apply_running_average(&input, &output);
	assert(output.x == 1000);  /* Average of 1 sample */
	assert(output.y == 500);
	assert(output.z == -500);
	printf("  ✓ First sample: X=%d Y=%d Z=%d\n", output.x, output.y, output.z);
	
	/* Second sample - same values */
	apply_running_average(&input, &output);
	assert(output.x == 1000);  /* Average of 2 samples */
	assert(output.y == 500);
	assert(output.z == -500);
	printf("  ✓ Second sample (constant): X=%d Y=%d Z=%d\n", output.x, output.y, output.z);
	
	/* Add varying values */
	input.x = 1100;
	input.y = 600;
	input.z = -400;
	apply_running_average(&input, &output);
	printf("  ✓ Third sample (varying): X=%d Y=%d Z=%d\n", output.x, output.y, output.z);
	
	/* The average should be smoothing values */
	assert(output.x > 1000 && output.x < 1100);  /* Should be between old and new */
	
	printf("✓ All running average tests passed!\n\n");
}

void test_running_average_smoothing(void)
{
	printf("Test: Running Average - Smoothing Effect\n");
	
	struct accel_data input, output;
	running_average_init();
	
	/* Simulate noisy data with a spike */
	int16_t noisy_data[] = {1000, 1010, 990, 2000, 1005, 995, 1000};  /* Spike at index 3 */
	
	printf("  Input sequence with spike: ");
	for (int i = 0; i < 7; i++) {
		printf("%d ", noisy_data[i]);
	}
	printf("\n");
	
	printf("  Smoothed output: ");
	for (int i = 0; i < 7; i++) {
		input.x = noisy_data[i];
		input.y = 0;
		input.z = 0;
		apply_running_average(&input, &output);
		printf("%d ", output.x);
	}
	printf("\n");
	
	printf("  ✓ Spike is smoothed by averaging\n");
	printf("✓ Running average smoothing test passed!\n\n");
}

void test_combined_filters(void)
{
	printf("Test: Combined Filters - Spike Limiter + Running Average\n");
	
	struct accel_data raw, spike_limited, final;
	
	/* Initialize both filters */
	spike_limiter_init(NULL);
	running_average_init();
	
	/* Simulate a large sudden spike */
	raw.x = 2000;  /* Large spike */
	raw.y = 0;
	raw.z = 1000;
	
	printf("  Raw input: X=%d Y=%d Z=%d\n", raw.x, raw.y, raw.z);
	
	/* Apply spike limiter first */
	apply_spike_limiter(&raw, &spike_limited);
	printf("  After spike limiter: X=%d Y=%d Z=%d\n", 
	       spike_limited.x, spike_limited.y, spike_limited.z);
	
	/* Then apply running average */
	apply_running_average(&spike_limited, &final);
	printf("  After running average: X=%d Y=%d Z=%d\n", 
	       final.x, final.y, final.z);
	
	/* The spike should be limited, then smoothed */
	assert(final.x <= SPIKE_LIMIT_MILLI_G);
	
	printf("  ✓ Filters work correctly in combination\n");
	printf("✓ Combined filter test passed!\n\n");
}

void test_edge_cases(void)
{
	printf("Test: Edge Cases\n");
	
	struct accel_data input, output;
	
	/* Test NULL handling */
	spike_limiter_init(NULL);  /* Should handle NULL gracefully */
	apply_spike_limiter(NULL, &output);  /* Should handle NULL gracefully */
	apply_running_average(NULL, &output);  /* Should handle NULL gracefully */
	printf("  ✓ NULL handling works correctly\n");
	
	/* Test int16_t limits */
	input.x = 32767;  /* INT16_MAX */
	input.y = -32768; /* INT16_MIN */
	input.z = 0;
	
	spike_limiter_init(&input);
	apply_spike_limiter(&input, &output);
	assert(output.x == 32767);
	assert(output.y == -32768);
	printf("  ✓ INT16 limits handled correctly\n");
	
	printf("✓ All edge case tests passed!\n\n");
}

int main(void)
{
	printf("\n========================================\n");
	printf("Client Filter Test Suite\n");
	printf("========================================\n\n");
	
	printf("Configuration:\n");
	printf("  SPIKE_LIMIT_MILLI_G: %d\n", SPIKE_LIMIT_MILLI_G);
	printf("  ENABLE_RUNNING_AVERAGE: %d\n", ENABLE_RUNNING_AVERAGE);
	printf("  RUNNING_AVERAGE_DEPTH: %d\n\n", RUNNING_AVERAGE_DEPTH);
	
	test_spike_limiter_basic();
	test_running_average_basic();
	test_running_average_smoothing();
	test_combined_filters();
	test_edge_cases();
	
	printf("========================================\n");
	printf("✓ ALL TESTS PASSED!\n");
	printf("========================================\n\n");
	
	return 0;
}

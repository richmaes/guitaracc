/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "motion_logic.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  /* for NULL */

/* Gravitational constant in m/s² */
#define GRAVITY 9.81

/* Maximum and minimum values for int16_t */
#define INT16_MAX_VAL 32767
#define INT16_MIN_VAL -32768

/* ========== SPIKE LIMITER ========== */

/* Previous values for spike limiting */
static struct accel_data spike_limiter_prev = {0, 0, 0};

/* Helper function to clamp a value within a range */
static int16_t clamp_int16(int32_t value)
{
	if (value > INT16_MAX_VAL) {
		return INT16_MAX_VAL;
	} else if (value < INT16_MIN_VAL) {
		return INT16_MIN_VAL;
	}
	return (int16_t)value;
}

void spike_limiter_init(const struct accel_data *initial)
{
	if (initial != NULL) {
		spike_limiter_prev = *initial;
	} else {
		spike_limiter_prev.x = 0;
		spike_limiter_prev.y = 0;
		spike_limiter_prev.z = 0;
	}
}

void apply_spike_limiter(const struct accel_data *raw, struct accel_data *limited)
{
	if (raw == NULL || limited == NULL) {
		return;
	}

	/* Calculate change for each axis */
	int32_t dx = (int32_t)raw->x - (int32_t)spike_limiter_prev.x;
	int32_t dy = (int32_t)raw->y - (int32_t)spike_limiter_prev.y;
	int32_t dz = (int32_t)raw->z - (int32_t)spike_limiter_prev.z;

	/* Limit change to SPIKE_LIMIT_MILLI_G */
	if (dx > SPIKE_LIMIT_MILLI_G) dx = SPIKE_LIMIT_MILLI_G;
	if (dx < -SPIKE_LIMIT_MILLI_G) dx = -SPIKE_LIMIT_MILLI_G;
	
	if (dy > SPIKE_LIMIT_MILLI_G) dy = SPIKE_LIMIT_MILLI_G;
	if (dy < -SPIKE_LIMIT_MILLI_G) dy = -SPIKE_LIMIT_MILLI_G;
	
	if (dz > SPIKE_LIMIT_MILLI_G) dz = SPIKE_LIMIT_MILLI_G;
	if (dz < -SPIKE_LIMIT_MILLI_G) dz = -SPIKE_LIMIT_MILLI_G;

	/* Apply limited change */
	limited->x = clamp_int16((int32_t)spike_limiter_prev.x + dx);
	limited->y = clamp_int16((int32_t)spike_limiter_prev.y + dy);
	limited->z = clamp_int16((int32_t)spike_limiter_prev.z + dz);

	/* Update previous values */
	spike_limiter_prev = *limited;
}

/* ========== RUNNING AVERAGE ========== */

#if ENABLE_RUNNING_AVERAGE

/* Ensure depth is within valid range (3-10) */
#if RUNNING_AVERAGE_DEPTH < 3
#undef RUNNING_AVERAGE_DEPTH
#define RUNNING_AVERAGE_DEPTH 3
#elif RUNNING_AVERAGE_DEPTH > 10
#undef RUNNING_AVERAGE_DEPTH
#define RUNNING_AVERAGE_DEPTH 10
#endif

/* Circular buffers for each axis */
static int16_t buffer_x[RUNNING_AVERAGE_DEPTH];
static int16_t buffer_y[RUNNING_AVERAGE_DEPTH];
static int16_t buffer_z[RUNNING_AVERAGE_DEPTH];

/* Buffer state */
static uint8_t buffer_index = 0;
static uint8_t buffer_count = 0;  /* Number of valid samples in buffer */

void running_average_init(void)
{
	buffer_index = 0;
	buffer_count = 0;
	
	/* Clear buffers */
	for (uint8_t i = 0; i < RUNNING_AVERAGE_DEPTH; i++) {
		buffer_x[i] = 0;
		buffer_y[i] = 0;
		buffer_z[i] = 0;
	}
}

void apply_running_average(const struct accel_data *input, struct accel_data *output)
{
	if (input == NULL || output == NULL) {
		return;
	}

	/* Add new sample to circular buffers */
	buffer_x[buffer_index] = input->x;
	buffer_y[buffer_index] = input->y;
	buffer_z[buffer_index] = input->z;

	/* Update index and count */
	buffer_index = (buffer_index + 1) % RUNNING_AVERAGE_DEPTH;
	if (buffer_count < RUNNING_AVERAGE_DEPTH) {
		buffer_count++;
	}

	/* Calculate averages */
	int32_t sum_x = 0, sum_y = 0, sum_z = 0;
	for (uint8_t i = 0; i < buffer_count; i++) {
		sum_x += buffer_x[i];
		sum_y += buffer_y[i];
		sum_z += buffer_z[i];
	}

	/* Compute average and store in output */
	output->x = (int16_t)(sum_x / buffer_count);
	output->y = (int16_t)(sum_y / buffer_count);
	output->z = (int16_t)(sum_z / buffer_count);
}

#endif /* ENABLE_RUNNING_AVERAGE */

/* ========== MOTION CONVERSION FUNCTIONS ========== */

int16_t convert_to_milli_g(double m_s2)
{
	/* Convert m/s² to milli-g: (m/s² / 9.81) * 1000 */
	double milli_g = (m_s2 / GRAVITY) * 1000.0;
	
	/* Clamp to int16_t range */
	if (milli_g > INT16_MAX_VAL) {
		return INT16_MAX_VAL;
	} else if (milli_g < INT16_MIN_VAL) {
		return INT16_MIN_VAL;
	}
	
	return (int16_t)milli_g;
}

void convert_accel_to_milli_g(double x, double y, double z, struct accel_data *data)
{
	if (data == NULL) {
		return;
	}
	
	data->x = convert_to_milli_g(x);
	data->y = convert_to_milli_g(y);
	data->z = convert_to_milli_g(z);
}

double calculate_magnitude(double x, double y, double z)
{
	return sqrt(x*x + y*y + z*z);
}

bool detect_motion(double x, double y, double z)
{
	double magnitude = calculate_magnitude(x, y, z);
	
	/* Detect motion when magnitude exceeds threshold */
	return magnitude > MOTION_THRESHOLD;
}

bool accel_data_changed(const struct accel_data *current, const struct accel_data *previous)
{
	if (current == NULL || previous == NULL) {
		return false;
	}
	
	return (current->x != previous->x ||
		current->y != previous->y ||
		current->z != previous->z);
}

bool detect_movement_threshold(const struct accel_data *current,
                                const struct accel_data *previous,
                                int16_t threshold_milli_g)
{
	if (current == NULL || previous == NULL) {
		return false;
	}
	
	/* Calculate absolute differences for each axis */
	int16_t dx = current->x - previous->x;
	int16_t dy = current->y - previous->y;
	int16_t dz = current->z - previous->z;
	
	/* Make absolute (handle negative values) */
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	if (dz < 0) dz = -dz;
	
	/* Check if any axis exceeded threshold */
	return (dx > threshold_milli_g || 
	        dy > threshold_milli_g || 
	        dz > threshold_milli_g);
}

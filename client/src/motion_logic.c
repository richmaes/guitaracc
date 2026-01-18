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

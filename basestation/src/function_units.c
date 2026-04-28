/*
 * Function Units Implementation
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "function_units.h"
#include <string.h>
#include <stdlib.h>

/* ========================================
 * HELPER FUNCTIONS
 * ======================================== */

/**
 * @brief Clamp a value to a range
 */
static inline int16_t clamp(int16_t value, int16_t min, int16_t max)
{
	if (value < min) {
		return min;
	}
	if (value > max) {
		return max;
	}
	return value;
}

/**
 * @brief Linear interpolation/mapping
 */
static int16_t linear_map(int16_t value, int16_t in_min, int16_t in_max, 
                          int16_t out_min, int16_t out_max)
{
	/* Handle edge cases */
	if (in_min == in_max) {
		return out_min;
	}
	
	/* Handle inverted input range (in_min > in_max) */
	bool inverted = (in_min > in_max);
	if (inverted) {
		/* Swap min and max for calculation */
		int16_t temp = in_min;
		in_min = in_max;
		in_max = temp;
		
		/* Also swap output range to maintain correct mapping */
		temp = out_min;
		out_min = out_max;
		out_max = temp;
	}
	
	/* Clamp input to range */
	if (value <= in_min) {
		return out_min;
	}
	if (value >= in_max) {
		return out_max;
	}
	
	/* Linear interpolation using integer math to avoid floats */
	int32_t in_range = (int32_t)in_max - (int32_t)in_min;
	int32_t out_range = (int32_t)out_max - (int32_t)out_min;
	int32_t in_offset = (int32_t)value - (int32_t)in_min;
	
	int32_t result = out_min + ((in_offset * out_range) / in_range);
	
	return (int16_t)result;
}

/* ========================================
 * PUBLIC API
 * ======================================== */

void func_init(struct function_unit *func, enum function_type type)
{
	if (!func) {
		return;
	}
	
	memset(func, 0, sizeof(*func));
	func->function_type = type;
	func->enabled = (type != FUNC_DISABLED) ? 1 : 0;
	
	/* Set default parameters based on type */
	switch (type) {
	case FUNC_LINEAR:
		/* Default: ±2g to full MIDI range */
		func->params[0] = -2000;  /* input_min */
		func->params[1] = 2000;   /* input_max */
		func->params[2] = 0;      /* output_min */
		func->params[3] = 127;    /* output_max */
		func->param_count = 4;
		break;
		
	case FUNC_DEADZONE:
		/* Default: 100 mg deadzone */
		func->params[0] = 100;
		func->param_count = 1;
		break;
		
	case FUNC_SCALE:
		/* Default: 100% scale (no change) */
		func->params[0] = 100;
		func->param_count = 1;
		break;
		
	case FUNC_CLAMP:
		/* Default: MIDI range */
		func->params[0] = 0;    /* min */
		func->params[1] = 127;  /* max */
		func->param_count = 2;
		break;
		
	case FUNC_SCALE_OFFSET:
		/* Default: sensor 0 (Accel X) */
		func->params[0] = 0;    /* sensor_index */
		func->param_count = 1;
		break;
		
	default:
		func->param_count = 0;
		break;
	}
}

void func_init_linear(struct function_unit *func, 
                      int16_t input_min, int16_t input_max,
                      int16_t output_min, int16_t output_max)
{
	if (!func) {
		return;
	}
	
	func_init(func, FUNC_LINEAR);
	func->params[0] = input_min;
	func->params[1] = input_max;
	func->params[2] = output_min;
	func->params[3] = output_max;
	func->param_count = 4;
}

void func_init_deadzone(struct function_unit *func, int16_t threshold)
{
	if (!func) {
		return;
	}
	
	func_init(func, FUNC_DEADZONE);
	func->params[0] = threshold;
	func->param_count = 1;
}

void func_init_scale_offset(struct function_unit *func, uint8_t sensor_index)
{
	if (!func) {
		return;
	}
	
	func_init(func, FUNC_SCALE_OFFSET);
	func->params[0] = (int16_t)sensor_index;
	func->param_count = 1;
}

int16_t func_process(const struct function_unit *func, int16_t input)
{
	if (!func || !func->enabled) {
		return 0;
	}
	
	switch (func->function_type) {
	case FUNC_PASSTHROUGH:
		return input;
		
	case FUNC_LINEAR:
		if (func->param_count >= 4) {
			return linear_map(input, 
			                  func->params[0], func->params[1],  /* input range */
			                  func->params[2], func->params[3]); /* output range */
		}
		return input;
		
	case FUNC_SCALE_OFFSET:
		/* NOTE: This function type requires access to global configuration
		 * (config.global.accel_scale[] and config.global.accel_offset[]).
		 * It should be processed at the topology processor level where
		 * global config is available, not in this generic func_process().
		 * 
		 * Recommended: Implement as pre-processing before topology execution,
		 * or use a specialized processing function that accepts global config.
		 * 
		 * This case returns input unchanged as a fallback.
		 */
		return input;
		
	case FUNC_DEADZONE:
		if (func->param_count >= 1) {
			int16_t threshold = func->params[0];
			if (abs(input) < threshold) {
				return 0;
			}
		}
		return input;
		
	case FUNC_INVERT:
		/* Assumes input is in MIDI range 0-127 */
		return 127 - clamp(input, 0, 127);
		
	case FUNC_SCALE:
		if (func->param_count >= 1) {
			/* Scale factor is fixed point: divide by 100 */
			int32_t scaled = ((int32_t)input * (int32_t)func->params[0]) / 100;
			return clamp((int16_t)scaled, 0, 127);
		}
		return input;
		
	case FUNC_CLAMP:
		if (func->param_count >= 2) {
			return clamp(input, func->params[0], func->params[1]);
		}
		return input;
		
	default:
		return 0;
	}
}

bool func_validate(const struct function_unit *func)
{
	if (!func) {
		return false;
	}
	
	/* Disabled is always valid */
	if (func->function_type == FUNC_DISABLED) {
		return true;
	}
	
	/* Check parameter count makes sense for function type */
	switch (func->function_type) {
	case FUNC_PASSTHROUGH:
	case FUNC_INVERT:
		return func->param_count == 0;
		
	case FUNC_DEADZONE:
	case FUNC_SCALE:
	case FUNC_SCALE_OFFSET:
		return func->param_count == 1;
		
	case FUNC_CLAMP:
		return func->param_count == 2;
		
	case FUNC_LINEAR:
		return func->param_count == 4;
		
	default:
		return false;
	}
}

const char *func_get_name(enum function_type type)
{
	switch (type) {
	case FUNC_DISABLED:
		return "Disabled";
	case FUNC_PASSTHROUGH:
		return "Passthrough";
	case FUNC_LINEAR:
		return "Linear";
	case FUNC_SCALE_OFFSET:
		return "Scale/Offset";
	case FUNC_DEADZONE:
		return "Deadzone";
	case FUNC_INVERT:
		return "Invert";
	case FUNC_SCALE:
		return "Scale";
	case FUNC_CLAMP:
		return "Clamp";
	case FUNC_CURVE_EXP:
		return "Exponential Curve";
	case FUNC_CURVE_LOG:
		return "Logarithmic Curve";
	case FUNC_LOWPASS:
		return "Low-pass Filter";
	default:
		return "Unknown";
	}
}

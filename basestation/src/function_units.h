/*
 * Function Units
 * Signal processing blocks for virtual port system
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FUNCTION_UNITS_H
#define FUNCTION_UNITS_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================
 * CONSTANTS
 * ======================================== */

#define FUNC_MAX_PARAMS     6    /* Maximum parameters per function */

/* ========================================
 * FUNCTION TYPES
 * ======================================== */

/**
 * @brief Function unit types
 * 
 * Each type performs a different signal processing operation.
 */
enum function_type {
	FUNC_DISABLED = 0,      /* Function unit not active */
	FUNC_PASSTHROUGH,       /* Direct copy (output = input) */
	FUNC_LINEAR,            /* Linear scaling with configurable range */
	FUNC_SCALE_OFFSET,      /* Global scale/offset calibration (reads from global config) */
	FUNC_DEADZONE,          /* Zero output below threshold */
	FUNC_INVERT,            /* Invert signal (127 - input) */
	FUNC_SCALE,             /* Multiply and clamp */
	FUNC_CLAMP,             /* Clamp to min/max range */
	/* Future function types */
	FUNC_CURVE_EXP,         /* Exponential curve */
	FUNC_CURVE_LOG,         /* Logarithmic curve */
	FUNC_LOWPASS,           /* Low-pass filter */
};

/* ========================================
 * DATA STRUCTURES
 * ======================================== */

/**
 * @brief Function unit configuration
 * 
 * Stores function type and parameters. Parameters are interpreted
 * differently based on function type.
 */
struct function_unit {
	uint8_t function_type;      /* enum function_type */
	uint8_t enabled;            /* Active flag */
	uint8_t param_count;        /* Number of valid parameters */
	uint8_t reserved;           /* Padding */
	int16_t params[FUNC_MAX_PARAMS]; /* Type-specific parameters */
} __attribute__((packed));

/* ========================================
 * PARAMETER DEFINITIONS BY FUNCTION TYPE
 * ======================================== */

/*
 * FUNC_PASSTHROUGH: No parameters
 *   output = input
 *
 * FUNC_LINEAR: params[0]=input_min, params[1]=input_max, 
 *              params[2]=output_min, params[3]=output_max
 *   Maps input range [input_min, input_max] to output range [output_min, output_max]
 *   Example: params={-2000, 2000, 0, 127} maps ±2g to full MIDI range
 *
 * FUNC_SCALE_OFFSET: params[0]=sensor_index (0-5)
 *   Applies global scale and offset calibration from config.global.accel_scale[] 
 *   and config.global.accel_offset[]. This is a shared calibration resource.
 *   Formula: output = ((input - offset) * 127) / scale
 *   Example: params={0} applies calibration for sensor 0 (Accel X)
 *   Note: Requires access to global configuration structure
 *
 * FUNC_DEADZONE: params[0]=threshold
 *   output = (abs(input) < threshold) ? 0 : input
 *   Example: params={10} zeros out small noise below ±10
 *
 * FUNC_INVERT: No parameters
 *   output = 127 - input
 *
 * FUNC_SCALE: params[0]=scale_factor (fixed point, divide by 100)
 *   output = (input * scale_factor) / 100, clamped to 0-127
 *   Example: params={150} multiplies by 1.5x
 *
 * FUNC_CLAMP: params[0]=min, params[1]=max
 *   output = clamp(input, min, max)
 */

/* ========================================
 * API FUNCTIONS
 * ======================================== */

/**
 * @brief Initialize a function unit with defaults
 * 
 * @param func Pointer to function unit
 * @param type Function type to configure
 */
void func_init(struct function_unit *func, enum function_type type);

/**
 * @brief Initialize a linear function unit
 * 
 * Convenience function for the common linear mapping case.
 * 
 * @param func Pointer to function unit
 * @param input_min Minimum input value
 * @param input_max Maximum input value
 * @param output_min Minimum output value (typically 0)
 * @param output_max Maximum output value (typically 127)
 */
void func_init_linear(struct function_unit *func, 
                      int16_t input_min, int16_t input_max,
                      int16_t output_min, int16_t output_max);

/**
 * @brief Initialize a scale/offset function unit
 * 
 * This function type applies global calibration from config.global.
 * 
 * @param func Pointer to function unit
 * @param sensor_index Sensor index (0-5) to calibrate
 */
void func_init_scale_offset(struct function_unit *func, uint8_t sensor_index);

/**
 * @brief Initialize a deadzone function unit
 * 
 * @param func Pointer to function unit
 * @param threshold Deadzone threshold (abs value)
 */
void func_init_deadzone(struct function_unit *func, int16_t threshold);

/**
 * @brief Process an input value through a function unit
 * 
 * Applies the function transformation to the input value.
 * 
 * @param func Pointer to function unit
 * @param input Input value
 * @return Processed output value
 */
int16_t func_process(const struct function_unit *func, int16_t input);

/**
 * @brief Validate function unit configuration
 * 
 * @param func Pointer to function unit
 * @return true if valid, false otherwise
 */
bool func_validate(const struct function_unit *func);

/**
 * @brief Get a human-readable name for a function type
 * 
 * @param type Function type enum value
 * @return String name, or "Unknown" for invalid types
 */
const char *func_get_name(enum function_type type);

#endif /* FUNCTION_UNITS_H */

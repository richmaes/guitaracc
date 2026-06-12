/*
 * Accelerometer Rotation Pipeline
 * 3D rotation, normalization, and configurable conversion to MIDI
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "accel_mapping.h"
#include <stddef.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Fixed input points for lookup table interpolation */
static const float lookup_input_points[LOOKUP_TABLE_POINTS] = {
	-1.0f, -0.5f, 0.0f, 0.5f, 1.0f
};

/* Helper to clamp float to range */
static inline float clampf(float value, float min, float max)
{
	if (value < min) return min;
	if (value > max) return max;
	return value;
}

/* Helper to clamp to MIDI range */
static inline uint8_t clamp_midi(int32_t value)
{
	if (value < MIDI_MIN_VALUE) return MIDI_MIN_VALUE;
	if (value > MIDI_MAX_VALUE) return MIDI_MAX_VALUE;
	return (uint8_t)value;
}

/* Convert degrees to radians */
static inline float deg_to_rad(float degrees)
{
	return degrees * M_PI / 180.0f;
}

void accel_rotation_init_defaults(struct accel_rotation_config *config)
{
	if (config == NULL) {
		return;
	}
	
	memset(config, 0, sizeof(*config));
	config->rho_angle = 0.0f;
	config->theta_angle = 0.0f;
	config->midi_cc = 1;  /* Default to CC 1 (modulation wheel) */
	config->func_type = CONV_FUNC_LINEAR;
	config->params.linear.scale = 1.0f;
	config->params.linear.offset = 0.0f;
}

void apply_rotation(struct vector3d *vec, float rho_deg, float theta_deg)
{
	if (vec == NULL) {
		return;
	}
	
	float rho_rad = deg_to_rad(rho_deg);
	float theta_rad = deg_to_rad(theta_deg);
	
	float cos_rho = cosf(rho_rad);
	float sin_rho = sinf(rho_rad);
	float cos_theta = cosf(theta_rad);
	float sin_theta = sinf(theta_rad);
	
	/* Rotation around X axis (rho) */
	float y1 = vec->y * cos_rho - vec->z * sin_rho;
	float z1 = vec->y * sin_rho + vec->z * cos_rho;
	vec->y = y1;
	vec->z = z1;
	
	/* Rotation around Y axis (theta) */
	float x2 = vec->x * cos_theta + vec->z * sin_theta;
	float z2 = -vec->x * sin_theta + vec->z * cos_theta;
	vec->x = x2;
	vec->z = z2;
}

bool normalize_vector(struct vector3d *vec)
{
	if (vec == NULL) {
		return false;
	}
	
	float magnitude = sqrtf(vec->x * vec->x + vec->y * vec->y + vec->z * vec->z);
	
	/* Avoid division by zero */
	if (magnitude < 1e-6f) {
		return false;
	}
	
	vec->x /= magnitude;
	vec->y /= magnitude;
	vec->z /= magnitude;
	
	return true;
}

uint8_t scalar_to_midi(float scalar, enum conversion_func_type func_type,
                       const union conversion_params *params)
{
	if (params == NULL) {
		return MIDI_MIN_VALUE;
	}
	
	float result = 0.0f;
	
	/* Clamp input scalar to valid range */
	scalar = clampf(scalar, -1.0f, 1.0f);
	
	switch (func_type) {
	case CONV_FUNC_LINEAR: {
		/* Linear: MIDI = clamp((scalar * scale + offset) * 127, 0, 127) */
		result = (scalar * params->linear.scale + params->linear.offset) * 127.0f;
		break;
	}
	
	case CONV_FUNC_EXPONENTIAL: {
		/* Exponential: MIDI = clamp(((scalar + 1)/2)^exponent * 127, 0, 127) */
		float normalized = (scalar + 1.0f) / 2.0f;  /* Map to [0, 1] */
		result = powf(normalized, params->exponential.exponent) * 127.0f;
		break;
	}
	
	case CONV_FUNC_SCURVE: {
		/* S-Curve: MIDI = clamp(127 / (1 + e^(-steepness * scalar)), 0, 127) */
		float exp_val = expf(-params->scurve.steepness * scalar);
		result = 127.0f / (1.0f + exp_val);
		break;
	}
	
	case CONV_FUNC_LOOKUP: {
		/* Lookup table with linear interpolation */
		/* Find the two surrounding points */
		int idx = 0;
		for (int i = 0; i < LOOKUP_TABLE_POINTS - 1; i++) {
			if (scalar >= lookup_input_points[i] && scalar <= lookup_input_points[i + 1]) {
				idx = i;
				break;
			}
		}
		
		/* Handle edge case: scalar exactly at last point */
		if (scalar >= lookup_input_points[LOOKUP_TABLE_POINTS - 1]) {
			return params->lookup.values[LOOKUP_TABLE_POINTS - 1];
		}
		
		/* Linear interpolation between two points */
		float x0 = lookup_input_points[idx];
		float x1 = lookup_input_points[idx + 1];
		float y0 = (float)params->lookup.values[idx];
		float y1 = (float)params->lookup.values[idx + 1];
		
		float t = (scalar - x0) / (x1 - x0);
		result = y0 + t * (y1 - y0);
		break;
	}
	
	default:
		/* Unknown function type, return mid-range */
		return 64;
	}
	
	/* Clamp to MIDI range and return */
	return clamp_midi((int32_t)(result + 0.5f));  /* Round to nearest */
}

bool validate_rotation_config(struct accel_rotation_config *config)
{
	if (config == NULL) {
		return false;
	}
	
	bool valid = true;
	
	/* Validate angles (wrap to 0-360) */
	while (config->rho_angle < 0.0f) {
		config->rho_angle += 360.0f;
		valid = false;
	}
	while (config->rho_angle >= 360.0f) {
		config->rho_angle -= 360.0f;
		valid = false;
	}
	
	while (config->theta_angle < 0.0f) {
		config->theta_angle += 360.0f;
		valid = false;
	}
	while (config->theta_angle >= 360.0f) {
		config->theta_angle -= 360.0f;
		valid = false;
	}
	
	/* Validate MIDI CC (clamp to 0-127) */
	if (config->midi_cc > 127) {
		config->midi_cc = 127;
		valid = false;
	}
	
	/* Validate function type */
	if (config->func_type > CONV_FUNC_LOOKUP) {
		config->func_type = CONV_FUNC_LINEAR;
		valid = false;
	}
	
	/* Validate function parameters */
	switch (config->func_type) {
	case CONV_FUNC_LINEAR:
		if (config->params.linear.scale < 0.1f || config->params.linear.scale > 10.0f) {
			config->params.linear.scale = clampf(config->params.linear.scale, 0.1f, 10.0f);
			valid = false;
		}
		if (config->params.linear.offset < -1.0f || config->params.linear.offset > 1.0f) {
			config->params.linear.offset = clampf(config->params.linear.offset, -1.0f, 1.0f);
			valid = false;
		}
		break;
		
	case CONV_FUNC_EXPONENTIAL:
		if (config->params.exponential.exponent < 0.1f || 
		    config->params.exponential.exponent > 5.0f) {
			config->params.exponential.exponent = 
				clampf(config->params.exponential.exponent, 0.1f, 5.0f);
			valid = false;
		}
		break;
		
	case CONV_FUNC_SCURVE:
		if (config->params.scurve.steepness < 1.0f || 
		    config->params.scurve.steepness > 20.0f) {
			config->params.scurve.steepness = 
				clampf(config->params.scurve.steepness, 1.0f, 20.0f);
			valid = false;
		}
		break;
		
	case CONV_FUNC_LOOKUP:
		/* Validate all lookup table values are in MIDI range */
		for (int i = 0; i < LOOKUP_TABLE_POINTS; i++) {
			if (config->params.lookup.values[i] > 127) {
				config->params.lookup.values[i] = 127;
				valid = false;
			}
		}
		break;
	}
	
	return valid;
}

uint8_t accel_rotation_pipeline(const struct accel_rotation_config *config,
                                 int16_t accel_x, int16_t accel_y, int16_t accel_z,
                                 uint8_t *midi_cc_out)
{
	if (config == NULL) {
		if (midi_cc_out != NULL) {
			*midi_cc_out = 1;  /* Default CC */
		}
		return MIDI_MIN_VALUE;
	}
	
	/* Stage 1: Convert milli-g to g and create vector */
	struct vector3d vec = {
		.x = (float)accel_x / 1000.0f,
		.y = (float)accel_y / 1000.0f,
		.z = (float)accel_z / 1000.0f
	};
	
	/* Stage 2: Apply angular rotation */
	apply_rotation(&vec, config->rho_angle, config->theta_angle);
	
	/* Stage 3: Normalize to unit vector */
	if (!normalize_vector(&vec)) {
		/* If normalization fails (zero vector), return mid-range */
		if (midi_cc_out != NULL) {
			*midi_cc_out = config->midi_cc;
		}
		return 64;
	}
	
	/* Stage 4: Project to 1D (X-axis component) */
	float scalar = vec.x;  /* Already in range [-1.0, 1.0] after normalization */
	
	/* Stage 5: Convert scalar to MIDI */
	uint8_t midi_value = scalar_to_midi(scalar, config->func_type, &config->params);
	
	/* Output the MIDI CC number */
	if (midi_cc_out != NULL) {
		*midi_cc_out = config->midi_cc;
	}
	
	return midi_value;
}

/* =============================================================================
 * Legacy Compatibility Functions (deprecated)
 * ============================================================================= */

uint8_t accel_map_to_midi(const struct accel_mapping_config *config, int16_t accel_value)
{
	if (config == NULL) {
		return MIDI_MIN_VALUE;
	}
	
	/* Handle edge case where min == max to avoid division by zero */
	if (config->accel_min == config->accel_max) {
		return MIDI_MIN_VALUE;
	}
	
	/* Calculate the range */
	int32_t accel_range = (int32_t)config->accel_max - (int32_t)config->accel_min;
	int32_t accel_offset = (int32_t)accel_value - (int32_t)config->accel_min;
	
	/* Linear mapping with proper handling of positive and negative ranges */
	int32_t midi_value = (accel_offset * (MIDI_MAX_VALUE - MIDI_MIN_VALUE)) / accel_range;
	midi_value += MIDI_MIN_VALUE;
	
	/* Clamp to valid MIDI range */
	if (midi_value < MIDI_MIN_VALUE) {
		midi_value = MIDI_MIN_VALUE;
	} else if (midi_value > MIDI_MAX_VALUE) {
		midi_value = MIDI_MAX_VALUE;
	}
	
	return (uint8_t)midi_value;
}

void accel_mapping_init_linear(struct accel_mapping_config *config, 
                                int16_t accel_min, 
                                int16_t accel_max)
{
	if (config == NULL) {
		return;
	}
	
	config->accel_min = accel_min;
	config->accel_max = accel_max;
}

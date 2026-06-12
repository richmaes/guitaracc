/*
 * Accelerometer Rotation Pipeline
 * 3D rotation, normalization, and configurable conversion to MIDI
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ACCEL_MAPPING_H
#define ACCEL_MAPPING_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

/* MIDI value range constants */
#define MIDI_MIN_VALUE 0
#define MIDI_MAX_VALUE 127

/* Number of lookup table points */
#define LOOKUP_TABLE_POINTS 5

/**
 * Conversion function types
 */
enum conversion_func_type {
	CONV_FUNC_LINEAR,      /* Linear scaling with offset */
	CONV_FUNC_EXPONENTIAL, /* Exponential curve */
	CONV_FUNC_SCURVE,      /* S-curve (sigmoid) */
	CONV_FUNC_LOOKUP       /* 5-point lookup table with linear interpolation */
};

/**
 * Conversion function parameters
 * Storage optimized for maximum 5 bytes per configuration
 */
union conversion_params {
	/* Linear: scale and offset */
	struct {
		float scale;   /* 0.1 to 10.0 */
		float offset;  /* -1.0 to 1.0 */
	} linear;
	
	/* Exponential: single exponent parameter */
	struct {
		float exponent; /* 0.1 to 5.0 */
	} exponential;
	
	/* S-curve: steepness parameter */
	struct {
		float steepness; /* 1.0 to 20.0 */
	} scurve;
	
	/* Lookup table: 5 output values at fixed input points */
	struct {
		uint8_t values[LOOKUP_TABLE_POINTS]; /* Output MIDI values 0-127 */
		/* Fixed input points: -1.0, -0.5, 0.0, 0.5, 1.0 */
	} lookup;
};

/**
 * Accelerometer rotation pipeline configuration
 * Total storage: 15 bytes maximum
 */
struct accel_rotation_config {
	float rho_angle;     /* Rotation around X axis (degrees, 0-360) - 4 bytes */
	float theta_angle;   /* Rotation around Y axis (degrees, 0-360) - 4 bytes */
	uint8_t midi_cc;     /* MIDI CC number (0-127) - 1 byte */
	enum conversion_func_type func_type;  /* Conversion function type - 1 byte */
	union conversion_params params;       /* Function parameters - 5 bytes max */
};

/**
 * 3D vector for accelerometer data
 */
struct vector3d {
	float x;
	float y;
	float z;
};

/**
 * Initialize rotation pipeline configuration with defaults
 * Defaults: rho=0, theta=0, CC=1, linear function (scale=1.0, offset=0.0)
 * 
 * @param config Pointer to configuration structure to initialize
 */
void accel_rotation_init_defaults(struct accel_rotation_config *config);

/**
 * Process accelerometer data through rotation pipeline
 * 
 * Pipeline stages:
 * 1. Angular rotation (X and Y axis)
 * 2. Unit vector normalization
 * 3. 1D projection (X-axis)
 * 4. Scalar to MIDI conversion
 * 
 * @param config Pipeline configuration
 * @param accel_x X-axis acceleration (milli-g)
 * @param accel_y Y-axis acceleration (milli-g)
 * @param accel_z Z-axis acceleration (milli-g)
 * @param midi_cc_out Output MIDI CC number (from config)
 * @return MIDI value (0-127), clamped to valid range
 */
uint8_t accel_rotation_pipeline(const struct accel_rotation_config *config,
                                 int16_t accel_x, int16_t accel_y, int16_t accel_z,
                                 uint8_t *midi_cc_out);

/**
 * Apply angular rotation to 3D vector
 * Rotates around X axis (rho), then Y axis (theta)
 * 
 * @param vec Input/output vector
 * @param rho_deg Rotation around X axis (degrees)
 * @param theta_deg Rotation around Y axis (degrees)
 */
void apply_rotation(struct vector3d *vec, float rho_deg, float theta_deg);

/**
 * Normalize vector to unit length
 * 
 * @param vec Input/output vector
 * @return true if normalization successful, false if vector is zero
 */
bool normalize_vector(struct vector3d *vec);

/**
 * Convert scalar value to MIDI using configured conversion function
 * 
 * @param scalar Input scalar value (-1.0 to 1.0)
 * @param func_type Conversion function type
 * @param params Function parameters
 * @return MIDI value (0-127), clamped to valid range
 */
uint8_t scalar_to_midi(float scalar, enum conversion_func_type func_type,
                       const union conversion_params *params);

/**
 * Validate and sanitize pipeline configuration parameters
 * Invalid parameters are corrected to safe defaults
 * 
 * @param config Configuration to validate
 * @return true if configuration was valid, false if corrections were made
 */
bool validate_rotation_config(struct accel_rotation_config *config);

/* Legacy compatibility - deprecated, use rotation pipeline instead */
struct accel_mapping_config {
	int16_t accel_min;
	int16_t accel_max;
};
uint8_t accel_map_to_midi(const struct accel_mapping_config *config, int16_t accel_value);
void accel_mapping_init_linear(struct accel_mapping_config *config, 
                                int16_t accel_min, int16_t accel_max);

#endif /* ACCEL_MAPPING_H */

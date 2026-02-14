/*
 * Accelerometer to MIDI Value Mapping
 * Configurable abstraction layer for translating accelerometer data to MIDI values
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ACCEL_MAPPING_H
#define ACCEL_MAPPING_H

#include <stdint.h>

/* MIDI value range constants */
#define MIDI_MIN_VALUE 0
#define MIDI_MAX_VALUE 127

/**
 * Mapping configuration structure
 * Defines how accelerometer values map to MIDI values
 */
struct accel_mapping_config {
	/* Linear mapping parameters */
	int16_t accel_min;  /* Accelerometer value that maps to MIDI 0 */
	int16_t accel_max;  /* Accelerometer value that maps to MIDI 127 */
	
	/* Future: could add other mapping types here */
	/* enum mapping_type { LINEAR, EXPONENTIAL, LOGARITHMIC }; */
};

/**
 * Apply accelerometer to MIDI value mapping
 * 
 * Maps an accelerometer value (in milli-g) to a MIDI value (0-127)
 * using the configured mapping parameters. Values outside the configured
 * range are clamped to MIDI_MIN_VALUE or MIDI_MAX_VALUE.
 * 
 * Linear mapping formula:
 *   midi_value = (accel_value - accel_min) * 127 / (accel_max - accel_min)
 * 
 * @param config Pointer to mapping configuration
 * @param accel_value Accelerometer value in milli-g
 * @return MIDI value (0-127), clamped to valid range
 */
uint8_t accel_map_to_midi(const struct accel_mapping_config *config, int16_t accel_value);

/**
 * Initialize a linear mapping configuration
 * 
 * Helper function to create a linear mapping between two accelerometer
 * values and the full MIDI range (0-127).
 * 
 * @param config Pointer to configuration structure to initialize
 * @param accel_min Accelerometer value that maps to MIDI 0
 * @param accel_max Accelerometer value that maps to MIDI 127
 */
void accel_mapping_init_linear(struct accel_mapping_config *config, 
                                int16_t accel_min, 
                                int16_t accel_max);

#endif /* ACCEL_MAPPING_H */

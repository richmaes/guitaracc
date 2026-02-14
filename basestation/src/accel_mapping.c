/*
 * Accelerometer to MIDI Value Mapping
 * Configurable abstraction layer for translating accelerometer data to MIDI values
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "accel_mapping.h"
#include <stddef.h>

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

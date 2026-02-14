/*
 * MIDI Logic Functions
 * Pure business logic with no hardware dependencies - can be tested on host
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "midi_logic.h"
#include "accel_mapping.h"
#include <stddef.h>

/* Default mapping configuration: ±2g range */
static struct accel_mapping_config default_mapping = {
	.accel_min = -2000,  /* -2000mg maps to MIDI 0 */
	.accel_max = 2000    /* +2000mg maps to MIDI 127 */
};

const struct accel_mapping_config *get_default_accel_mapping(void)
{
	return &default_mapping;
}

uint8_t accel_to_midi_cc(int16_t milli_g, const struct accel_mapping_config *config)
{
	/* Use default mapping if none provided */
	if (config == NULL) {
		config = &default_mapping;
	}
	
	return accel_map_to_midi(config, milli_g);
}

void construct_midi_cc_msg(uint8_t channel, uint8_t cc_number, uint8_t value, uint8_t *msg_out)
{
	/* MIDI CC message format:
	 * Byte 0: 0xB0 + channel (0xB0 = CC on channel 0)
	 * Byte 1: CC number (0-127)
	 * Byte 2: CC value (0-127)
	 */
	msg_out[0] = 0xB0 | (channel & 0x0F);
	msg_out[1] = cc_number & 0x7F;
	msg_out[2] = value & 0x7F;
}

/*
 * MIDI Logic Functions
 * Pure business logic with no hardware dependencies - can be tested on host
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "midi_logic.h"

uint8_t accel_to_midi_cc(int16_t milli_g)
{
	/* Input range: approximately -2000 to +2000 milli-g (±2g)
	 * Output range: 0 to 127 (MIDI CC value)
	 * Map -2000 -> 0, 0 -> 64, +2000 -> 127
	 */
	int32_t value = ((int32_t)milli_g + 2000) * 127 / 4000;
	
	/* Clamp to valid MIDI CC range */
	if (value < 0) value = 0;
	if (value > 127) value = 127;
	
	return (uint8_t)value;
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

/*
 * MIDI Logic Functions
 * Pure business logic with no hardware dependencies - can be tested on host
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MIDI_LOGIC_H
#define MIDI_LOGIC_H

#include <stdint.h>

/* MIDI Continuous Controller numbers */
#define MIDI_CC_X_AXIS 16  /* General Purpose Controller 1 */
#define MIDI_CC_Y_AXIS 17  /* General Purpose Controller 2 */
#define MIDI_CC_Z_AXIS 18  /* General Purpose Controller 3 */

/* Acceleration data structure: X, Y, Z in milli-g */
struct accel_data {
	int16_t x;  /* X-axis in milli-g */
	int16_t y;  /* Y-axis in milli-g */
	int16_t z;  /* Z-axis in milli-g */
} __attribute__((packed));

/**
 * Convert milli-g value to MIDI CC value (0-127)
 * 
 * @param milli_g Acceleration in milli-g (±2000 range)
 * @return MIDI CC value (0-127), clamped to valid range
 */
uint8_t accel_to_midi_cc(int16_t milli_g);

/**
 * Construct a MIDI Control Change message
 * 
 * @param channel MIDI channel (0-15)
 * @param cc_number Controller number (0-127)
 * @param value Controller value (0-127)
 * @param msg_out Output buffer (must be at least 3 bytes)
 */
void construct_midi_cc_msg(uint8_t channel, uint8_t cc_number, uint8_t value, uint8_t *msg_out);

#endif /* MIDI_LOGIC_H */

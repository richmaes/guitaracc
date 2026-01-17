/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basestation Emulator for Integration Testing
 * Uses actual midi_logic.c for business logic
 */

#ifndef BASESTATION_EMULATOR_H
#define BASESTATION_EMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_hal.h"
#include "common_defs.h"

#define MAX_GUITARS 4

/* MIDI message output buffer */
typedef struct {
	uint8_t msg[3];
	bool valid;
} midi_output_t;

/* Guitar connection info */
typedef struct {
	bool connected;
	ble_conn_handle_t handle;
	uint8_t addr[6];
	struct accel_data last_accel;
} guitar_info_t;

/* Basestation state */
typedef struct {
	bool initialized;
	bool scanning;
	
	/* Guitar connections */
	guitar_info_t guitars[MAX_GUITARS];
	int num_guitars;
	
	/* Last MIDI outputs (for verification) */
	midi_output_t last_midi_x;
	midi_output_t last_midi_y;
	midi_output_t last_midi_z;
	
	/* Statistics */
	uint32_t packets_received;
	uint32_t midi_messages_sent;
} basestation_emulator_t;

/**
 * @brief Initialize basestation emulator
 * 
 * @param base Basestation emulator instance
 * @return 0 on success, negative errno on failure
 */
int basestation_emulator_init(basestation_emulator_t *base);

/**
 * @brief Start scanning for guitars
 * 
 * @param base Basestation emulator instance
 * @return 0 on success, negative errno on failure
 */
int basestation_emulator_start_scan(basestation_emulator_t *base);

/**
 * @brief Stop scanning
 * 
 * @param base Basestation emulator instance
 * @return 0 on success, negative errno on failure
 */
int basestation_emulator_stop_scan(basestation_emulator_t *base);

/**
 * @brief Connect to a discovered guitar
 * 
 * @param base Basestation emulator instance
 * @param addr Guitar device address
 * @return 0 on success, negative errno on failure
 */
int basestation_emulator_connect(basestation_emulator_t *base, const uint8_t *addr);

/**
 * @brief Enable notifications from a guitar
 * 
 * @param base Basestation emulator instance
 * @param guitar_index Guitar index (0-3)
 * @return 0 on success, negative errno on failure
 */
int basestation_emulator_enable_notifications(basestation_emulator_t *base, int guitar_index);

/**
 * @brief Get last MIDI output for verification
 * 
 * @param base Basestation emulator instance
 * @param axis Axis (0=X, 1=Y, 2=Z)
 * @param msg Output buffer for MIDI message (3 bytes)
 * @return true if valid MIDI message available, false otherwise
 */
bool basestation_emulator_get_last_midi(const basestation_emulator_t *base,
                                        int axis, uint8_t *msg);

/**
 * @brief Get number of connected guitars
 * 
 * @param base Basestation emulator instance
 * @return Number of connected guitars
 */
int basestation_emulator_get_num_guitars(const basestation_emulator_t *base);

/**
 * @brief Cleanup basestation emulator
 * 
 * @param base Basestation emulator instance
 */
void basestation_emulator_cleanup(basestation_emulator_t *base);

/**
 * @brief Dump basestation state (for debugging)
 * 
 * @param base Basestation emulator instance
 */
void basestation_emulator_dump_state(const basestation_emulator_t *base);

#endif /* BASESTATION_EMULATOR_H */

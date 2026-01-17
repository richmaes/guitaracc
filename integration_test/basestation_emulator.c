/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Basestation Emulator Implementation
 */

#include "basestation_emulator.h"
#include <string.h>
#include <stdio.h>

/* MIDI CC numbers (from midi_logic.h) */
#define MIDI_CC_X_AXIS 16
#define MIDI_CC_Y_AXIS 17
#define MIDI_CC_Z_AXIS 18

/* External midi_logic functions (implemented in midi_logic.c) */
extern uint8_t accel_to_midi_cc(int16_t milli_g);
extern void construct_midi_cc_msg(uint8_t channel, uint8_t cc_number, uint8_t value, uint8_t *msg_out);

/* GATT Characteristic handle for acceleration data */
#define ACCEL_CHAR_HANDLE 1

/* Guitar Service UUID (to match against) */
static const uint8_t guitar_service_uuid[] = {
	0xa7, 0xc8, 0xf9, 0xd2, 0x4b, 0x3e, 0x4a, 0x1d,
	0x9f, 0x2c, 0x8e, 0x7d, 0x6c, 0x5b, 0x4a, 0x3f
};

/* BLE callbacks */
static void basestation_scan_cb(const uint8_t *addr, const ble_adv_data_t *adv_data);
static void basestation_connected_cb(ble_conn_handle_t handle);
static void basestation_disconnected_cb(ble_conn_handle_t handle, uint8_t reason);
static void basestation_notify_cb(ble_conn_handle_t handle, ble_gatt_handle_t char_handle,
                                  const void *data, size_t len);

/* Global reference for callbacks */
static basestation_emulator_t *g_base = NULL;

/* Helper to find guitar by handle */
static guitar_info_t* find_guitar_by_handle(basestation_emulator_t *base, ble_conn_handle_t handle)
{
	for (int i = 0; i < base->num_guitars; i++) {
		if (base->guitars[i].connected && base->guitars[i].handle == handle) {
			return &base->guitars[i];
		}
	}
	return NULL;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int basestation_emulator_init(basestation_emulator_t *base)
{
	if (!base) {
		return -1;
	}
	
	memset(base, 0, sizeof(basestation_emulator_t));
	base->initialized = true;
	g_base = base;
	
	return 0;
}

void basestation_emulator_cleanup(basestation_emulator_t *base)
{
	if (!base || !base->initialized) {
		return;
	}
	
	if (base->scanning) {
		basestation_emulator_stop_scan(base);
	}
	
	/* Disconnect all guitars */
	for (int i = 0; i < base->num_guitars; i++) {
		if (base->guitars[i].connected) {
			ble_hal_disconnect(base->guitars[i].handle);
		}
	}
	
	g_base = NULL;
	memset(base, 0, sizeof(basestation_emulator_t));
}

/* ============================================================================
 * Scanning
 * ============================================================================ */

int basestation_emulator_start_scan(basestation_emulator_t *base)
{
	if (!base || !base->initialized) {
		return -1;
	}
	
	int err = ble_hal_scan_start(basestation_scan_cb);
	if (err == 0) {
		base->scanning = true;
		printf("[BASESTATION] Scanning started\n");
	}
	
	return err;
}

int basestation_emulator_stop_scan(basestation_emulator_t *base)
{
	if (!base || !base->initialized) {
		return -1;
	}
	
	int err = ble_hal_scan_stop();
	if (err == 0) {
		base->scanning = false;
		printf("[BASESTATION] Scanning stopped\n");
	}
	
	return err;
}

static void basestation_scan_cb(const uint8_t *addr, const ble_adv_data_t *adv_data)
{
	if (!g_base) {
		return;
	}
	
	/* Check if this is a Guitar device (has our service UUID) */
	bool is_guitar = false;
	
	/* Simple scan for our UUID in advertisement data */
	for (size_t i = 0; i < adv_data->len - 16; i++) {
		if (adv_data->data[i] == 0x07 && /* Complete 128-bit UUID list */
		    memcmp(&adv_data->data[i+1], guitar_service_uuid, 16) == 0) {
			is_guitar = true;
			break;
		}
	}
	
	if (is_guitar) {
		printf("[BASESTATION] Discovered guitar: %02X:%02X:%02X:%02X:%02X:%02X\n",
		       addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
	}
}

/* ============================================================================
 * Connection Management
 * ============================================================================ */

int basestation_emulator_connect(basestation_emulator_t *base, const uint8_t *addr)
{
	if (!base || !base->initialized) {
		return -1;
	}
	
	if (base->num_guitars >= MAX_GUITARS) {
		printf("[BASESTATION] Max guitars reached\n");
		return -2;
	}
	
	ble_conn_handle_t handle = ble_hal_connect(addr, 
	                                           basestation_connected_cb,
	                                           basestation_disconnected_cb);
	
	if (handle == BLE_CONN_HANDLE_INVALID) {
		return -3;
	}
	
	/* Add to guitar list */
	guitar_info_t *guitar = &base->guitars[base->num_guitars++];
	guitar->connected = true;
	guitar->handle = handle;
	memcpy(guitar->addr, addr, 6);
	
	return 0;
}

static void basestation_connected_cb(ble_conn_handle_t handle)
{
	printf("[BASESTATION] Connected to guitar (handle %d)\n", handle);
}

static void basestation_disconnected_cb(ble_conn_handle_t handle, uint8_t reason)
{
	if (!g_base) {
		return;
	}
	
	printf("[BASESTATION] Guitar disconnected (handle %d, reason 0x%02X)\n", 
	       handle, reason);
	
	/* Remove from guitar list */
	for (int i = 0; i < g_base->num_guitars; i++) {
		if (g_base->guitars[i].handle == handle) {
			/* Shift remaining guitars down */
			for (int j = i; j < g_base->num_guitars - 1; j++) {
				g_base->guitars[j] = g_base->guitars[j + 1];
			}
			g_base->num_guitars--;
			break;
		}
	}
}

/* ============================================================================
 * Notification Handling
 * ============================================================================ */

int basestation_emulator_enable_notifications(basestation_emulator_t *base, int guitar_index)
{
	if (!base || !base->initialized) {
		return -1;
	}
	
	if (guitar_index < 0 || guitar_index >= base->num_guitars) {
		return -2;
	}
	
	guitar_info_t *guitar = &base->guitars[guitar_index];
	if (!guitar->connected) {
		return -3;
	}
	
	int err = ble_hal_notify_enable(guitar->handle, ACCEL_CHAR_HANDLE,
	                                basestation_notify_cb);
	
	if (err == 0) {
		printf("[BASESTATION] Notifications enabled for guitar %d\n", guitar_index);
	}
	
	return err;
}

static void basestation_notify_cb(ble_conn_handle_t handle, ble_gatt_handle_t char_handle,
                                  const void *data, size_t len)
{
	if (!g_base || len != sizeof(struct accel_data)) {
		return;
	}
	
	guitar_info_t *guitar = find_guitar_by_handle(g_base, handle);
	if (!guitar) {
		return;
	}
	
	/* Copy acceleration data */
	const struct accel_data *accel = (const struct accel_data *)data;
	guitar->last_accel = *accel;
	g_base->packets_received++;
	
	/* Convert to MIDI using actual midi_logic */
	uint8_t midi_x = accel_to_midi_cc(accel->x);
	uint8_t midi_y = accel_to_midi_cc(accel->y);
	uint8_t midi_z = accel_to_midi_cc(accel->z);
	
	/* Construct MIDI CC messages */
	construct_midi_cc_msg(0, MIDI_CC_X_AXIS, midi_x, g_base->last_midi_x.msg);
	construct_midi_cc_msg(0, MIDI_CC_Y_AXIS, midi_y, g_base->last_midi_y.msg);
	construct_midi_cc_msg(0, MIDI_CC_Z_AXIS, midi_z, g_base->last_midi_z.msg);
	
	g_base->last_midi_x.valid = true;
	g_base->last_midi_y.valid = true;
	g_base->last_midi_z.valid = true;
	g_base->midi_messages_sent += 3;
	
	printf("[BASESTATION] Received accel: X=%d, Y=%d, Z=%d milli-g -> MIDI: X=%d, Y=%d, Z=%d\n",
	       accel->x, accel->y, accel->z, midi_x, midi_y, midi_z);
}

/* ============================================================================
 * Query Functions
 * ============================================================================ */

bool basestation_emulator_get_last_midi(const basestation_emulator_t *base,
                                        int axis, uint8_t *msg)
{
	if (!base || !msg) {
		return false;
	}
	
	const midi_output_t *output = NULL;
	
	switch (axis) {
	case 0: output = &base->last_midi_x; break;
	case 1: output = &base->last_midi_y; break;
	case 2: output = &base->last_midi_z; break;
	default: return false;
	}
	
	if (!output->valid) {
		return false;
	}
	
	memcpy(msg, output->msg, 3);
	return true;
}

int basestation_emulator_get_num_guitars(const basestation_emulator_t *base)
{
	return base ? base->num_guitars : 0;
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

void basestation_emulator_dump_state(const basestation_emulator_t *base)
{
	if (!base) {
		return;
	}
	
	printf("\n=== Basestation Emulator State ===\n");
	printf("Initialized: %s\n", base->initialized ? "Yes" : "No");
	printf("Scanning:    %s\n", base->scanning ? "Yes" : "No");
	printf("Guitars:     %d connected\n", base->num_guitars);
	
	for (int i = 0; i < base->num_guitars; i++) {
		guitar_info_t *g = &base->guitars[i];
		printf("  [%d] Addr=%02X:%02X:%02X:%02X:%02X:%02X, Handle=%d\n",
		       i, g->addr[0], g->addr[1], g->addr[2],
		       g->addr[3], g->addr[4], g->addr[5], g->handle);
	}
	
	printf("\nLast MIDI Output:\n");
	if (base->last_midi_x.valid) {
		printf("  X-axis: [0x%02X 0x%02X 0x%02X]\n",
		       base->last_midi_x.msg[0], base->last_midi_x.msg[1], base->last_midi_x.msg[2]);
	}
	if (base->last_midi_y.valid) {
		printf("  Y-axis: [0x%02X 0x%02X 0x%02X]\n",
		       base->last_midi_y.msg[0], base->last_midi_y.msg[1], base->last_midi_y.msg[2]);
	}
	if (base->last_midi_z.valid) {
		printf("  Z-axis: [0x%02X 0x%02X 0x%02X]\n",
		       base->last_midi_z.msg[0], base->last_midi_z.msg[1], base->last_midi_z.msg[2]);
	}
	
	printf("\nStatistics:\n");
	printf("  Packets received:     %u\n", base->packets_received);
	printf("  MIDI messages sent:   %u\n", base->midi_messages_sent);
	printf("===================================\n\n");
}

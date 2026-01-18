/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Client Emulator Implementation
 */

#include "client_emulator.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* External motion_logic functions (implemented in motion_logic.c) */
extern void convert_accel_to_milli_g(double x, double y, double z, struct accel_data *data);
extern bool accel_data_changed(const struct accel_data *new_data, 
                               const struct accel_data *old_data);
extern bool detect_motion(double x, double y, double z);

/* GATT Characteristic handle for acceleration data */
#define ACCEL_CHAR_HANDLE 1

/* Guitar Service UUID (for advertising) */
static const uint8_t guitar_service_uuid[] = {
	0xa7, 0xc8, 0xf9, 0xd2, 0x4b, 0x3e, 0x4a, 0x1d,
	0x9f, 0x2c, 0x8e, 0x7d, 0x6c, 0x5b, 0x4a, 0x3f
};

/* BLE connection callbacks */
static void client_connected_cb(ble_conn_handle_t handle, const uint8_t *addr);
static void client_disconnected_cb(ble_conn_handle_t handle, uint8_t reason);
static void client_notify_enabled_cb(ble_conn_handle_t handle, ble_gatt_handle_t char_handle);

/* Global reference for callbacks */
static client_emulator_t *g_client = NULL;

/* ============================================================================
 * Initialization
 * ============================================================================ */

int client_emulator_init(client_emulator_t *client, const uint8_t *addr)
{
	if (!client) {
		return -1;
	}
	
	memset(client, 0, sizeof(client_emulator_t));
	client->conn_handle = BLE_CONN_HANDLE_INVALID;
	
	if (addr) {
		memcpy(client->addr, addr, 6);
	} else {
		/* Generate random address */
		for (int i = 0; i < 6; i++) {
			client->addr[i] = rand() & 0xFF;
		}
		client->addr[0] |= 0xC0;  /* Random static address */
	}
	
	client->initialized = true;
	g_client = client;
	
	return 0;
}

void client_emulator_cleanup(client_emulator_t *client)
{
	if (!client || !client->initialized) {
		return;
	}
	
	if (client->advertising) {
		client_emulator_stop_advertising(client);
	}
	
	if (client->connected) {
		ble_hal_disconnect(client->conn_handle);
	}
	
	g_client = NULL;
	memset(client, 0, sizeof(client_emulator_t));
}

/* ============================================================================
 * Advertising
 * ============================================================================ */

int client_emulator_start_advertising(client_emulator_t *client)
{
	if (!client || !client->initialized) {
		return -1;
	}
	
	/* Register peripheral callbacks first */
	int err = ble_hal_peripheral_register_callbacks(client->addr,
	                                                 client_connected_cb,
	                                                 client_disconnected_cb,
	                                                 client_notify_enabled_cb);
	if (err != 0) {
		return err;
	}
	
	/* Build advertisement data with Guitar Service UUID */
	uint8_t adv_data[31];
	int offset = 0;
	
	/* Flags */
	adv_data[offset++] = 2;  /* Length */
	adv_data[offset++] = 0x01;  /* Type: Flags */
	adv_data[offset++] = 0x06;  /* General Discoverable + BR/EDR not supported */
	
	/* 128-bit Service UUID */
	adv_data[offset++] = 17;  /* Length */
	adv_data[offset++] = 0x07;  /* Type: Complete list of 128-bit UUIDs */
	memcpy(&adv_data[offset], guitar_service_uuid, 16);
	offset += 16;
	
	ble_adv_data_t adv = {
		.data = adv_data,
		.len = offset
	};
	
	err = ble_hal_adv_start(client->addr, &adv);
	if (err == 0) {
		client->advertising = true;
	}
	
	return err;
}

int client_emulator_stop_advertising(client_emulator_t *client)
{
	if (!client || !client->initialized) {
		return -1;
	}
	
	int err = ble_hal_adv_stop();
	if (err == 0) {
		client->advertising = false;
	}
	
	return err;
}

/* ============================================================================
 * BLE Connection Callbacks
 * ============================================================================ */

static void client_connected_cb(ble_conn_handle_t handle, const uint8_t *addr)
{
	if (!g_client) {
		return;
	}
	
	g_client->connected = true;
	g_client->conn_handle = handle;
	g_client->advertising = false;  /* Stop advertising when connected */
	
	printf("[CLIENT] Connected (handle %d) to basestation %02X:%02X:%02X:%02X:%02X:%02X\n",
	       handle, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void client_disconnected_cb(ble_conn_handle_t handle, uint8_t reason)
{
	if (!g_client) {
		return;
	}
	
	printf("[CLIENT] Disconnected (handle %d, reason 0x%02X)\n", handle, reason);
	
	g_client->connected = false;
	g_client->conn_handle = BLE_CONN_HANDLE_INVALID;
	g_client->notify_enabled = false;
}

static void client_notify_enabled_cb(ble_conn_handle_t handle, ble_gatt_handle_t char_handle)
{
	if (!g_client) {
		return;
	}
	
	(void)char_handle;  /* Unused - we only have one characteristic */
	
	g_client->notify_enabled = true;
	
	printf("[CLIENT] Notifications enabled (handle %d)\n", handle);
}

/* ============================================================================
 * Acceleration Data Handling
 * ============================================================================ */

int client_emulator_update_accel(client_emulator_t *client, 
                                  double x, double y, double z)
{
	if (!client || !client->initialized) {
		return -1;
	}
	
	/* Check motion threshold before processing */
	if (!detect_motion(x, y, z)) {
		client->notifications_skipped++;
		return 0;  /* Motion below threshold, skip */
	}
	
	/* Convert to milli-g using actual motion_logic */
	convert_accel_to_milli_g(x, y, z, &client->current_accel);
	
	/* Check if we should send notification */
	if (!client->connected) {
		return 0;  /* Not connected, nothing to do */
	}
	
	if (!client->notify_enabled) {
		return 0;  /* Notifications not enabled */
	}
	
	/* Only send if data changed (power optimization) */
	if (!accel_data_changed(&client->current_accel, &client->previous_accel)) {
		client->notifications_skipped++;
		return 0;
	}
	
	/* Send notification */
	int err = ble_hal_notify(client->conn_handle, ACCEL_CHAR_HANDLE,
	                         &client->current_accel, sizeof(client->current_accel));
	
	if (err == 0) {
		client->previous_accel = client->current_accel;
		client->notifications_sent++;
	}
	
	return err;
}

bool client_emulator_is_connected(const client_emulator_t *client)
{
	return client && client->initialized && client->connected;
}

void client_emulator_get_address(const client_emulator_t *client, uint8_t *addr)
{
	if (client && addr) {
		memcpy(addr, client->addr, 6);
	}
}

int client_emulator_send_accel(client_emulator_t *client, 
                                const struct accel_data *accel)
{
	if (!client || !client->initialized || !accel) {
		return -1;
	}
	
	if (!client->connected || !client->notify_enabled) {
		client->notifications_skipped++;
		return -2;
	}
	
	/* Send notification with acceleration data */
	int err = ble_hal_notify(client->conn_handle, ACCEL_CHAR_HANDLE,
	                         accel, sizeof(*accel));
	
	if (err == 0) {
		client->current_accel = *accel;
		client->notifications_sent++;
	}
	
	return err;
}

void client_emulator_get_accel(const client_emulator_t *client, 
                               struct accel_data *data)
{
	if (client && data) {
		*data = client->current_accel;
	}
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

void client_emulator_dump_state(const client_emulator_t *client)
{
	if (!client) {
		return;
	}
	
	printf("\n=== Client Emulator State ===\n");
	printf("Initialized:  %s\n", client->initialized ? "Yes" : "No");
	printf("Advertising:  %s\n", client->advertising ? "Yes" : "No");
	printf("Connected:    %s\n", client->connected ? "Yes" : "No");
	printf("Notify:       %s\n", client->notify_enabled ? "Enabled" : "Disabled");
	
	if (client->connected) {
		printf("Conn Handle:  %d\n", client->conn_handle);
	}
	
	printf("\nCurrent Accel: X=%d, Y=%d, Z=%d milli-g\n",
	       client->current_accel.x,
	       client->current_accel.y,
	       client->current_accel.z);
	
	printf("\nStatistics:\n");
	printf("  Notifications sent:    %u\n", client->notifications_sent);
	printf("  Notifications skipped: %u\n", client->notifications_skipped);
	printf("==============================\n\n");
}

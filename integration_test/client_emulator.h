/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Client Emulator for Integration Testing
 * Uses actual motion_logic.c for business logic
 */

#ifndef CLIENT_EMULATOR_H
#define CLIENT_EMULATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_hal.h"
#include "common_defs.h"

/* Client state */
typedef struct {
	bool initialized;
	bool advertising;
	bool connected;
	ble_conn_handle_t conn_handle;
	uint8_t addr[6];  /* BLE address */
	
	/* Acceleration data */
	struct accel_data current_accel;
	struct accel_data previous_accel;
	
	/* Connection state */
	bool notify_enabled;
	
	/* Statistics */
	uint32_t notifications_sent;
	uint32_t notifications_skipped;  /* Due to no change */
} client_emulator_t;

/**
 * @brief Initialize client emulator
 * 
 * @param client Client emulator instance
 * @param addr BLE address to use (6 bytes), or NULL for random
 * @return 0 on success, negative errno on failure
 */
int client_emulator_init(client_emulator_t *client, const uint8_t *addr);

/**
 * @brief Start advertising
 * 
 * @param client Client emulator instance
 * @return 0 on success, negative errno on failure
 */
int client_emulator_start_advertising(client_emulator_t *client);

/**
 * @brief Stop advertising
 * 
 * @param client Client emulator instance
 * @return 0 on success, negative errno on failure
 */
int client_emulator_stop_advertising(client_emulator_t *client);

/**
 * @brief Update acceleration data
 * 
 * Converts m/s² to milli-g using motion_logic and sends notification
 * if data changed and connected.
 * 
 * @param client Client emulator instance
 * @param x X-axis acceleration in m/s²
 * @param y Y-axis acceleration in m/s²
 * @param z Z-axis acceleration in m/s²
 * @return 0 on success, negative errno on failure
 */
int client_emulator_update_accel(client_emulator_t *client, 
                                  double x, double y, double z);

/**
 * @brief Send acceleration data directly
 * 
 * Sends already-converted milli-g data. Useful for testing specific values.
 * 
 * @param client Client emulator instance
 * @param accel Acceleration data in milli-g
 * @return 0 on success, negative errno on failure
 */
int client_emulator_send_accel(client_emulator_t *client, 
                                const struct accel_data *accel);

/**
 * @brief Check if connected to basestation
 * 
 * @param client Client emulator instance
 * @return true if connected, false otherwise
 */
bool client_emulator_is_connected(const client_emulator_t *client);

/**
 * @brief Get client BLE address
 * 
 * @param client Client emulator instance
 * @param addr Output buffer for address (6 bytes)
 */
void client_emulator_get_address(const client_emulator_t *client, uint8_t *addr);

/**
 * @brief Get current acceleration data
 * 
 * @param client Client emulator instance
 * @param data Output buffer for acceleration data
 */
void client_emulator_get_accel(const client_emulator_t *client, 
                               struct accel_data *data);

/**
 * @brief Cleanup client emulator
 * 
 * @param client Client emulator instance
 */
void client_emulator_cleanup(client_emulator_t *client);

/**
 * @brief Dump client state (for debugging)
 * 
 * @param client Client emulator instance
 */
void client_emulator_dump_state(const client_emulator_t *client);

#endif /* CLIENT_EMULATOR_H */

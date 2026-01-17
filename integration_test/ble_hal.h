/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE Hardware Abstraction Layer for Integration Testing
 * 
 * This provides a platform-independent interface for BLE operations,
 * allowing the same business logic to run on both real hardware (Zephyr)
 * and in the host-based integration test environment.
 */

#ifndef BLE_HAL_H
#define BLE_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * BLE Connection Management
 * ============================================================================ */

/* Connection handle type */
typedef uint16_t ble_conn_handle_t;
#define BLE_CONN_HANDLE_INVALID 0xFFFF

/* Connection states */
typedef enum {
	BLE_CONN_STATE_DISCONNECTED,
	BLE_CONN_STATE_CONNECTING,
	BLE_CONN_STATE_CONNECTED
} ble_conn_state_t;

/* Connection callbacks */
typedef void (*ble_connected_cb_t)(ble_conn_handle_t handle);
typedef void (*ble_disconnected_cb_t)(ble_conn_handle_t handle, uint8_t reason);

/* ============================================================================
 * BLE Advertising (Peripheral Role)
 * ============================================================================ */

/* Advertisement data */
typedef struct {
	uint8_t *data;
	size_t len;
} ble_adv_data_t;

/* Forward declarations for peripheral callbacks (defined after ble_gatt_handle_t) */
typedef struct ble_peripheral_callbacks ble_peripheral_callbacks_t;
typedef void (*ble_peripheral_connected_cb_t)(ble_conn_handle_t handle, const uint8_t *addr);
typedef void (*ble_peripheral_disconnected_cb_t)(ble_conn_handle_t handle, uint8_t reason);

/**
 * @brief Register peripheral connection callbacks
 * 
 * Called by peripherals (advertising devices) to receive notifications
 * when centrals connect/disconnect.
 * 
 * @param addr Peripheral's BLE address (6 bytes)
 * @param connected_cb Callback when central connects
 * @param disconnected_cb Callback when central disconnects
 * @param notify_enabled_cb Callback when central enables notifications (can be NULL, requires ble_gatt_handle_t)
 * @return 0 on success, negative errno on failure
 */
int ble_hal_peripheral_register_callbacks(const uint8_t *addr,
                                           ble_peripheral_connected_cb_t connected_cb,
                                           ble_peripheral_disconnected_cb_t disconnected_cb,
                                           void *notify_enabled_cb);

/**
 * @brief Start BLE advertising
 * 
 * @param addr BLE address to advertise from (6 bytes)
 * @param adv_data Advertisement data to broadcast
 * @return 0 on success, negative errno on failure
 */
int ble_hal_adv_start(const uint8_t *addr, const ble_adv_data_t *adv_data);

/**
 * @brief Stop BLE advertising
 * 
 * @return 0 on success, negative errno on failure
 */
int ble_hal_adv_stop(void);

/* ============================================================================
 * BLE Scanning (Central Role)
 * ============================================================================ */

/* Scan result callback */
typedef void (*ble_scan_cb_t)(const uint8_t *addr, const ble_adv_data_t *adv_data);

/**
 * @brief Start BLE scanning
 * 
 * @param callback Function called for each discovered device
 * @return 0 on success, negative errno on failure
 */
int ble_hal_scan_start(ble_scan_cb_t callback);

/**
 * @brief Stop BLE scanning
 * 
 * @return 0 on success, negative errno on failure
 */
int ble_hal_scan_stop(void);

/* ============================================================================
 * BLE Connection (Central Role)
 * ============================================================================ */

/**
 * @brief Connect to a BLE peripheral
 * 
 * @param addr Device address to connect to
 * @param connected_cb Callback when connection established
 * @param disconnected_cb Callback when connection lost
 * @return Connection handle on success, BLE_CONN_HANDLE_INVALID on failure
 */
ble_conn_handle_t ble_hal_connect(const uint8_t *addr,
                                   ble_connected_cb_t connected_cb,
                                   ble_disconnected_cb_t disconnected_cb);

/**
 * @brief Disconnect from a BLE peripheral
 * 
 * @param handle Connection handle
 * @return 0 on success, negative errno on failure
 */
int ble_hal_disconnect(ble_conn_handle_t handle);

/* ============================================================================
 * GATT Notifications (Peripheral Role)
 * ============================================================================ */

/* GATT characteristic handle */
typedef uint16_t ble_gatt_handle_t;

/* Peripheral connection callbacks (for devices being connected to) */
typedef void (*ble_peripheral_connected_cb_t)(ble_conn_handle_t handle, const uint8_t *addr);
typedef void (*ble_peripheral_disconnected_cb_t)(ble_conn_handle_t handle, uint8_t reason);
typedef void (*ble_peripheral_notify_enabled_cb_t)(ble_conn_handle_t handle, ble_gatt_handle_t char_handle);

/**
 * @brief Check if notifications are enabled for a characteristic
 * 
 * @param handle Connection handle
 * @param char_handle Characteristic handle
 * @return true if enabled, false otherwise
 */
bool ble_hal_notify_enabled(ble_conn_handle_t handle, ble_gatt_handle_t char_handle);

/**
 * @brief Send a GATT notification
 * 
 * @param handle Connection handle
 * @param char_handle Characteristic handle
 * @param data Data to send
 * @param len Length of data
 * @return 0 on success, negative errno on failure
 */
int ble_hal_notify(ble_conn_handle_t handle, ble_gatt_handle_t char_handle,
                   const void *data, size_t len);

/* ============================================================================
 * GATT Notifications (Central Role - Receiving)
 * ============================================================================ */

/* Notification receive callback */
typedef void (*ble_notify_rx_cb_t)(ble_conn_handle_t handle, 
                                   ble_gatt_handle_t char_handle,
                                   const void *data, size_t len);

/**
 * @brief Enable notifications for a characteristic
 * 
 * @param handle Connection handle
 * @param char_handle Characteristic handle
 * @param callback Function called when notification received
 * @return 0 on success, negative errno on failure
 */
int ble_hal_notify_enable(ble_conn_handle_t handle, ble_gatt_handle_t char_handle,
                          ble_notify_rx_cb_t callback);

/**
 * @brief Disable notifications for a characteristic
 * 
 * @param handle Connection handle
 * @param char_handle Characteristic handle
 * @return 0 on success, negative errno on failure
 */
int ble_hal_notify_disable(ble_conn_handle_t handle, ble_gatt_handle_t char_handle);

/* ============================================================================
 * Message Queue (for event processing in test environment)
 * ============================================================================ */

/**
 * @brief Process pending BLE events
 * 
 * This should be called periodically in test environment to simulate
 * event-driven behavior. Processes message queue and triggers callbacks.
 * 
 * @return Number of events processed
 */
int ble_hal_process_events(void);

/**
 * @brief Get number of pending events in queue
 * 
 * @return Number of pending events
 */
int ble_hal_pending_events(void);

/* ============================================================================
 * Initialization and Cleanup
 * ============================================================================ */

/**
 * @brief Initialize BLE HAL
 * 
 * Must be called before any other BLE HAL functions.
 * In test environment, sets up message queue.
 * 
 * @return 0 on success, negative errno on failure
 */
int ble_hal_init(void);

/**
 * @brief Cleanup BLE HAL
 * 
 * Frees all resources, disconnects all connections.
 * Should be called at end of test.
 * 
 * @return 0 on success, negative errno on failure
 */
int ble_hal_cleanup(void);

/* ============================================================================
 * Test/Debug Functions
 * ============================================================================ */

/**
 * @brief Get connection state
 * 
 * @param handle Connection handle
 * @return Connection state
 */
ble_conn_state_t ble_hal_get_conn_state(ble_conn_handle_t handle);

/**
 * @brief Dump BLE HAL state (for debugging)
 */
void ble_hal_dump_state(void);

#endif /* BLE_HAL_H */

/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLE Hardware Abstraction Layer Implementation
 * Message Queue-based BLE simulation for integration testing
 */

#include "ble_hal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

#define MAX_CONNECTIONS 4
#define MAX_DEVICES 10
#define MAX_EVENTS 100
#define MAX_ADV_DATA 31

/* Event types */
typedef enum {
	EVENT_NONE,
	EVENT_ADV_START,
	EVENT_ADV_STOP,
	EVENT_SCAN_RESULT,
	EVENT_CONNECT,
	EVENT_CONNECTED,
	EVENT_DISCONNECT,
	EVENT_DISCONNECTED,
	EVENT_NOTIFY_TX,
	EVENT_NOTIFY_RX,
	EVENT_NOTIFY_ENABLE,
} ble_event_type_t;

/* BLE event in message queue */
typedef struct {
	ble_event_type_t type;
	ble_conn_handle_t handle;
	uint8_t addr[6];
	ble_gatt_handle_t char_handle;
	uint8_t data[256];
	size_t data_len;
	uint8_t reason;
} ble_event_t;

/* Device information (for scanning) */
typedef struct {
	uint8_t addr[6];
	uint8_t adv_data[MAX_ADV_DATA];
	size_t adv_data_len;
	bool advertising;
	/* Peripheral callbacks */
	ble_peripheral_connected_cb_t peripheral_connected_cb;
	ble_peripheral_disconnected_cb_t peripheral_disconnected_cb;
	ble_peripheral_notify_enabled_cb_t peripheral_notify_enabled_cb;
} ble_device_t;

/* Connection information */
typedef struct {
	bool in_use;
	ble_conn_state_t state;
	uint8_t addr[6];
	ble_connected_cb_t connected_cb;
	ble_disconnected_cb_t disconnected_cb;
	ble_notify_rx_cb_t notify_rx_cb;
	ble_gatt_handle_t notify_char_handle;
	bool notify_enabled;
} ble_connection_t;

/* Global state */
static struct {
	bool initialized;
	
	/* Advertising */
	bool advertising;
	uint8_t adv_data[MAX_ADV_DATA];
	size_t adv_data_len;
	uint8_t own_addr[6];
	
	/* Scanning */
	bool scanning;
	ble_scan_cb_t scan_cb;
	
	/* Connections */
	ble_connection_t connections[MAX_CONNECTIONS];
	
	/* Discovered devices */
	ble_device_t devices[MAX_DEVICES];
	int num_devices;
	
	/* Message queue */
	ble_event_t event_queue[MAX_EVENTS];
	int queue_head;
	int queue_tail;
	int queue_count;
	
} ble_state;

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

static void enqueue_event(const ble_event_t *event)
{
	if (ble_state.queue_count >= MAX_EVENTS) {
		printf("WARNING: BLE event queue full, dropping event\n");
		return;
	}
	
	ble_state.event_queue[ble_state.queue_tail] = *event;
	ble_state.queue_tail = (ble_state.queue_tail + 1) % MAX_EVENTS;
	ble_state.queue_count++;
}

static bool dequeue_event(ble_event_t *event)
{
	if (ble_state.queue_count == 0) {
		return false;
	}
	
	*event = ble_state.event_queue[ble_state.queue_head];
	ble_state.queue_head = (ble_state.queue_head + 1) % MAX_EVENTS;
	ble_state.queue_count--;
	return true;
}

static ble_conn_handle_t allocate_connection(void)
{
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (!ble_state.connections[i].in_use) {
			ble_state.connections[i].in_use = true;
			return i;
		}
	}
	return BLE_CONN_HANDLE_INVALID;
}

static void free_connection(ble_conn_handle_t handle)
{
	if (handle < MAX_CONNECTIONS) {
		memset(&ble_state.connections[handle], 0, sizeof(ble_connection_t));
	}
}

static ble_device_t* find_device_by_addr(const uint8_t *addr)
{
	for (int i = 0; i < ble_state.num_devices; i++) {
		if (memcmp(ble_state.devices[i].addr, addr, 6) == 0) {
			return &ble_state.devices[i];
		}
	}
	return NULL;
}

static ble_device_t* add_device(const uint8_t *addr)
{
	if (ble_state.num_devices >= MAX_DEVICES) {
		return NULL;
	}
	
	ble_device_t *dev = &ble_state.devices[ble_state.num_devices++];
	memcpy(dev->addr, addr, 6);
	return dev;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int ble_hal_init(void)
{
	memset(&ble_state, 0, sizeof(ble_state));
	
	/* Generate a random address for this device */
	for (int i = 0; i < 6; i++) {
		ble_state.own_addr[i] = rand() & 0xFF;
	}
	ble_state.own_addr[0] |= 0xC0;  /* Set to random static address */
	
	ble_state.initialized = true;
	return 0;
}

int ble_hal_cleanup(void)
{
	/* Disconnect all connections */
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (ble_state.connections[i].in_use) {
			ble_hal_disconnect(i);
		}
	}
	
	/* Clear event queue */
	ble_state.queue_head = 0;
	ble_state.queue_tail = 0;
	ble_state.queue_count = 0;
	
	ble_state.initialized = false;
	return 0;
}

/* ============================================================================
 * Advertising
 * ============================================================================ */

int ble_hal_peripheral_register_callbacks(const uint8_t *addr,
                                           ble_peripheral_connected_cb_t connected_cb,
                                           ble_peripheral_disconnected_cb_t disconnected_cb,
                                           void *notify_enabled_cb)
{
	if (!ble_state.initialized || !addr) {
		return -1;
	}
	
	/* Find or create device */
	ble_device_t *dev = find_device_by_addr(addr);
	if (!dev) {
		dev = add_device(addr);
	}
	if (!dev) {
		return -2;
	}
	
	/* Register callbacks */
	dev->peripheral_connected_cb = connected_cb;
	dev->peripheral_disconnected_cb = disconnected_cb;
	dev->peripheral_notify_enabled_cb = (ble_peripheral_notify_enabled_cb_t)notify_enabled_cb;
	
	return 0;
}

int ble_hal_adv_start(const uint8_t *addr, const ble_adv_data_t *adv_data)
{
	if (!ble_state.initialized || !addr) {
		return -1;
	}
	
	if (adv_data->len > MAX_ADV_DATA) {
		return -2;
	}
	
	/* Register as discoverable device at the specified address */
	ble_device_t *dev = find_device_by_addr(addr);
	if (!dev) {
		dev = add_device(addr);
	}
	if (!dev) {
		return -3;
	}
	
	/* Set advertisement data */
	memcpy(dev->adv_data, adv_data->data, adv_data->len);
	dev->adv_data_len = adv_data->len;
	dev->advertising = true;
	
	/* Post event so scanners can discover us */
	ble_event_t event = {0};
	event.type = EVENT_ADV_START;
	memcpy(event.addr, addr, 6);
	enqueue_event(&event);
	
	return 0;
}

int ble_hal_adv_stop(void)
{
	if (!ble_state.initialized) {
		return -1;
	}
	
	ble_state.advertising = false;
	
	ble_device_t *dev = find_device_by_addr(ble_state.own_addr);
	if (dev) {
		dev->advertising = false;
	}
	
	return 0;
}

/* ============================================================================
 * Scanning
 * ============================================================================ */

int ble_hal_scan_start(ble_scan_cb_t callback)
{
	if (!ble_state.initialized || !callback) {
		return -1;
	}
	
	ble_state.scanning = true;
	ble_state.scan_cb = callback;
	
	/* Immediately report any currently advertising devices */
	for (int i = 0; i < ble_state.num_devices; i++) {
		if (ble_state.devices[i].advertising) {
			ble_adv_data_t adv_data = {
				.data = ble_state.devices[i].adv_data,
				.len = ble_state.devices[i].adv_data_len
			};
			ble_state.scan_cb(ble_state.devices[i].addr, &adv_data);
		}
	}
	
	return 0;
}

int ble_hal_scan_stop(void)
{
	if (!ble_state.initialized) {
		return -1;
	}
	
	ble_state.scanning = false;
	ble_state.scan_cb = NULL;
	return 0;
}

/* ============================================================================
 * Connection Management
 * ============================================================================ */

ble_conn_handle_t ble_hal_connect(const uint8_t *addr,
                                   ble_connected_cb_t connected_cb,
                                   ble_disconnected_cb_t disconnected_cb)
{
	if (!ble_state.initialized) {
		return BLE_CONN_HANDLE_INVALID;
	}
	
	/* Check if device exists and is advertising */
	ble_device_t *dev = find_device_by_addr(addr);
	if (!dev || !dev->advertising) {
		return BLE_CONN_HANDLE_INVALID;
	}
	
	/* Allocate connection */
	ble_conn_handle_t handle = allocate_connection();
	if (handle == BLE_CONN_HANDLE_INVALID) {
		return BLE_CONN_HANDLE_INVALID;
	}
	
	ble_connection_t *conn = &ble_state.connections[handle];
	conn->state = BLE_CONN_STATE_CONNECTING;
	memcpy(conn->addr, addr, 6);
	conn->connected_cb = connected_cb;
	conn->disconnected_cb = disconnected_cb;
	
	/* Post connection event */
	ble_event_t event = {0};
	event.type = EVENT_CONNECTED;
	event.handle = handle;
	memcpy(event.addr, addr, 6);
	enqueue_event(&event);
	
	return handle;
}

int ble_hal_disconnect(ble_conn_handle_t handle)
{
	if (!ble_state.initialized || handle >= MAX_CONNECTIONS) {
		return -1;
	}
	
	ble_connection_t *conn = &ble_state.connections[handle];
	if (!conn->in_use) {
		return -2;
	}
	
	/* Post disconnection event */
	ble_event_t event = {0};
	event.type = EVENT_DISCONNECTED;
	event.handle = handle;
	event.reason = 0x16;  /* Connection terminated by local host */
	enqueue_event(&event);
	
	return 0;
}

/* ============================================================================
 * GATT Notifications
 * ============================================================================ */

bool ble_hal_notify_enabled(ble_conn_handle_t handle, ble_gatt_handle_t char_handle)
{
	if (!ble_state.initialized || handle >= MAX_CONNECTIONS) {
		return false;
	}
	
	ble_connection_t *conn = &ble_state.connections[handle];
	if (!conn->in_use || conn->state != BLE_CONN_STATE_CONNECTED) {
		return false;
	}
	
	return conn->notify_enabled && (conn->notify_char_handle == char_handle);
}

int ble_hal_notify(ble_conn_handle_t handle, ble_gatt_handle_t char_handle,
                   const void *data, size_t len)
{
	if (!ble_state.initialized || handle >= MAX_CONNECTIONS) {
		return -1;
	}
	
	ble_connection_t *conn = &ble_state.connections[handle];
	if (!conn->in_use || conn->state != BLE_CONN_STATE_CONNECTED) {
		return -2;
	}
	
	if (!conn->notify_enabled || conn->notify_char_handle != char_handle) {
		return -3;  /* Notifications not enabled */
	}
	
	if (len > sizeof(((ble_event_t*)0)->data)) {
		return -4;  /* Data too large */
	}
	
	/* Post notification event */
	ble_event_t event = {0};
	event.type = EVENT_NOTIFY_RX;
	event.handle = handle;
	event.char_handle = char_handle;
	memcpy(event.data, data, len);
	event.data_len = len;
	enqueue_event(&event);
	
	return 0;
}

int ble_hal_notify_enable(ble_conn_handle_t handle, ble_gatt_handle_t char_handle,
                          ble_notify_rx_cb_t callback)
{
	if (!ble_state.initialized || handle >= MAX_CONNECTIONS || !callback) {
		return -1;
	}
	
	ble_connection_t *conn = &ble_state.connections[handle];
	if (!conn->in_use || conn->state != BLE_CONN_STATE_CONNECTED) {
		return -2;
	}
	
	conn->notify_enabled = true;
	conn->notify_char_handle = char_handle;
	conn->notify_rx_cb = callback;
	
	/* Post event to notify peripheral */
	ble_event_t event = {0};
	event.type = EVENT_NOTIFY_ENABLE;
	event.handle = handle;
	event.char_handle = char_handle;
	memcpy(event.addr, conn->addr, 6);
	enqueue_event(&event);
	
	return 0;
}

int ble_hal_notify_disable(ble_conn_handle_t handle, ble_gatt_handle_t char_handle)
{
	if (!ble_state.initialized || handle >= MAX_CONNECTIONS) {
		return -1;
	}
	
	ble_connection_t *conn = &ble_state.connections[handle];
	if (!conn->in_use) {
		return -2;
	}
	
	if (conn->notify_char_handle == char_handle) {
		conn->notify_enabled = false;
		conn->notify_rx_cb = NULL;
	}
	
	return 0;
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

int ble_hal_process_events(void)
{
	if (!ble_state.initialized) {
		return -1;
	}
	
	int processed = 0;
	ble_event_t event;
	
	while (dequeue_event(&event)) {
		processed++;
		
		switch (event.type) {
		case EVENT_CONNECTED:
			if (event.handle < MAX_CONNECTIONS) {
				ble_connection_t *conn = &ble_state.connections[event.handle];
				conn->state = BLE_CONN_STATE_CONNECTED;
				
				/* Notify central (initiator) */
				if (conn->connected_cb) {
					conn->connected_cb(event.handle);
				}
				
				/* Notify peripheral (acceptor) */
				ble_device_t *peripheral = find_device_by_addr(conn->addr);
				if (peripheral && peripheral->peripheral_connected_cb) {
					peripheral->peripheral_connected_cb(event.handle, conn->addr);
				}
			}
			break;
			
		case EVENT_DISCONNECTED:
			if (event.handle < MAX_CONNECTIONS) {
				ble_connection_t *conn = &ble_state.connections[event.handle];
				conn->state = BLE_CONN_STATE_DISCONNECTED;
				
				/* Notify central */
				if (conn->disconnected_cb) {
					conn->disconnected_cb(event.handle, event.reason);
				}
				
				/* Notify peripheral */
				ble_device_t *peripheral = find_device_by_addr(conn->addr);
				if (peripheral && peripheral->peripheral_disconnected_cb) {
					peripheral->peripheral_disconnected_cb(event.handle, event.reason);
				}
				
				free_connection(event.handle);
			}
			break;
			
		case EVENT_NOTIFY_RX:
			if (event.handle < MAX_CONNECTIONS) {
				ble_connection_t *conn = &ble_state.connections[event.handle];
				if (conn->notify_rx_cb) {
					conn->notify_rx_cb(event.handle, event.char_handle,
					                  event.data, event.data_len);
				}
			}
			break;
			
		case EVENT_ADV_START:
			/* Notify scanners */
			if (ble_state.scanning && ble_state.scan_cb) {
				ble_device_t *dev = find_device_by_addr(event.addr);
				if (dev && dev->advertising) {
					ble_adv_data_t adv_data = {
						.data = dev->adv_data,
						.len = dev->adv_data_len
					};
					ble_state.scan_cb(dev->addr, &adv_data);
				}
			}
			break;
			
		case EVENT_NOTIFY_ENABLE:
			/* Notify peripheral that notifications were enabled */
			{
				ble_device_t *peripheral = find_device_by_addr(event.addr);
				if (peripheral && peripheral->peripheral_notify_enabled_cb) {
					peripheral->peripheral_notify_enabled_cb(event.handle, event.char_handle);
				}
			}
			break;
			
		default:
			break;
		}
	}
	
	return processed;
}

int ble_hal_pending_events(void)
{
	return ble_state.queue_count;
}

/* ============================================================================
 * Debug Functions
 * ============================================================================ */

ble_conn_state_t ble_hal_get_conn_state(ble_conn_handle_t handle)
{
	if (!ble_state.initialized || handle >= MAX_CONNECTIONS) {
		return BLE_CONN_STATE_DISCONNECTED;
	}
	
	return ble_state.connections[handle].state;
}

void ble_hal_dump_state(void)
{
	printf("\n=== BLE HAL State ===\n");
	printf("Initialized: %s\n", ble_state.initialized ? "Yes" : "No");
	printf("Advertising: %s\n", ble_state.advertising ? "Yes" : "No");
	printf("Scanning:    %s\n", ble_state.scanning ? "Yes" : "No");
	printf("Devices:     %d\n", ble_state.num_devices);
	printf("Events:      %d pending\n", ble_state.queue_count);
	
	printf("\nConnections:\n");
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (ble_state.connections[i].in_use) {
			ble_connection_t *conn = &ble_state.connections[i];
			printf("  [%d] State=%d, Addr=%02X:%02X:%02X:%02X:%02X:%02X, Notify=%s\n",
			       i, conn->state,
			       conn->addr[0], conn->addr[1], conn->addr[2],
			       conn->addr[3], conn->addr[4], conn->addr[5],
			       conn->notify_enabled ? "Enabled" : "Disabled");
		}
	}
	printf("====================\n\n");
}

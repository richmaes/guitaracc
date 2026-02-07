/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/drivers/uart.h>
#include "midi_logic.h"

LOG_MODULE_REGISTER(basestation, LOG_LEVEL_DBG);

/* Debug flags */
#define MIDI_DEBUG 0  /* Enable detailed MIDI transmission logging */
#define BLE_DEBUG 1   /* Enable BLE connection and discovery logging */

/* Enable test mode to send periodic MIDI test messages */
#define TEST_MODE_ENABLED 1
#define TEST_INTERVAL_MS 1000

/* Set to 1 for testing via VCOM at 115200, 0 for real MIDI at 31250 */
/* NOTE: Baud rate is set in app.overlay, not here. This define is unused. */
// #define USE_VCOM_BAUD 1

#define MAX_GUITARS 4

/* Custom Guitar Service UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f */
#define BT_UUID_GUITAR_SERVICE_VAL \
	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a3f)

/* Acceleration Data Characteristic UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a40 */
#define BT_UUID_GUITAR_ACCEL_CHAR_VAL \
	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a40)

static struct bt_uuid_128 guitar_service_uuid = BT_UUID_INIT_128(
	BT_UUID_GUITAR_SERVICE_VAL);

static struct bt_uuid_128 guitar_accel_char_uuid = BT_UUID_INIT_128(
	BT_UUID_GUITAR_ACCEL_CHAR_VAL);

struct guitar_connection {
	struct bt_conn *conn;
	uint16_t accel_handle;
	bool subscribed;
};

static struct guitar_connection guitar_conns[MAX_GUITARS];
static const struct device *midi_uart;

/* MIDI TX queue for interrupt-driven transmission */
#define MIDI_TX_QUEUE_SIZE 256
static uint8_t midi_tx_queue[MIDI_TX_QUEUE_SIZE];
static volatile size_t midi_tx_head = 0;
static volatile size_t midi_tx_tail = 0;
static K_SEM_DEFINE(midi_tx_sem, 0, 1);

static void uart_isr(const struct device *dev, void *user_data)
{
	uart_irq_update(dev);
	
	if (uart_irq_tx_ready(dev)) {
		if (midi_tx_tail != midi_tx_head) {
			/* Get next byte from queue */
			uint8_t byte = midi_tx_queue[midi_tx_tail];
			midi_tx_tail = (midi_tx_tail + 1) % MIDI_TX_QUEUE_SIZE;
			
			/* Send byte */
#if MIDI_DEBUG
			int sent = uart_fifo_fill(dev, &byte, 1);
			LOG_DBG("UART ISR: sent byte 0x%02x (result=%d)", byte, sent);
#else
			uart_fifo_fill(dev, &byte, 1);
#endif
		} else {
			/* Queue empty, disable TX interrupt */
			uart_irq_tx_disable(dev);
			k_sem_give(&midi_tx_sem);
#if MIDI_DEBUG
			LOG_DBG("UART ISR: queue empty, TX disabled");
#endif
		}
	}
	
	/* Drain RX FIFO if needed (we don't use RX, but clear it to prevent issues) */
	if (uart_irq_rx_ready(dev)) {
		uint8_t dummy;
		while (uart_fifo_read(dev, &dummy, 1) > 0) {
			/* Discard received data */
		}
	}
}

static int queue_midi_bytes(const uint8_t *data, size_t len)
{
	if (!midi_uart) {
		return -ENODEV;
	}
	
	/* Add bytes to queue */
	for (size_t i = 0; i < len; i++) {
		size_t next_head = (midi_tx_head + 1) % MIDI_TX_QUEUE_SIZE;
		if (next_head == midi_tx_tail) {
			LOG_WRN("MIDI TX queue full, dropping bytes");
			return -ENOMEM;
		}
		midi_tx_queue[midi_tx_head] = data[i];
		midi_tx_head = next_head;
	}
	
#if MIDI_DEBUG
	LOG_DBG("Queued %d bytes, head=%d tail=%d", len, midi_tx_head, midi_tx_tail);
#endif
	
	/* Enable TX interrupt to start transmission */
	uart_irq_tx_enable(midi_uart);
	
	return 0;
}

static void send_midi_cc(uint8_t channel, uint8_t cc_number, uint8_t value)
{
	uint8_t midi_msg[3];
	
	/* Construct MIDI CC message using shared logic */
	construct_midi_cc_msg(channel, cc_number, value, midi_msg);
	
	/* Queue for interrupt-driven transmission */
	queue_midi_bytes(midi_msg, 3);
	
#if MIDI_DEBUG
	LOG_DBG("MIDI CC ch=%d, cc=%d, val=%d", channel, cc_number, value);
#endif
}

static void process_accel_data(const struct accel_data *data, uint8_t guitar_id)
{
	uint8_t cc_x, cc_y, cc_z;
	
	/* Convert acceleration to MIDI CC values */
	cc_x = accel_to_midi_cc(data->x);
	cc_y = accel_to_midi_cc(data->y);
	cc_z = accel_to_midi_cc(data->z);
	
	LOG_INF("Guitar %d: X=%d Y=%d Z=%d milli-g -> MIDI: X=%d Y=%d Z=%d",
		guitar_id, data->x, data->y, data->z, cc_x, cc_y, cc_z);
	
	/* Send MIDI CC messages on channel 0 (can be per-guitar if needed) */
	send_midi_cc(0, MIDI_CC_X_AXIS, cc_x);
	send_midi_cc(0, MIDI_CC_Y_AXIS, cc_y);
	send_midi_cc(0, MIDI_CC_Z_AXIS, cc_z);
}

static uint8_t accel_notify_callback(struct bt_conn *conn,
				     struct bt_gatt_subscribe_params *params,
				     const void *data, uint16_t length)
{
	const struct accel_data *accel;
	int guitar_id = -1;
	
	if (!data) {
		LOG_INF("Unsubscribed from acceleration notifications");
		params->value_handle = 0;
		return BT_GATT_ITER_STOP;
	}
	
	if (length != sizeof(struct accel_data)) {
		LOG_ERR("Invalid acceleration data length: %d", length);
		return BT_GATT_ITER_CONTINUE;
	}
	
	/* Find which guitar this is */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (guitar_conns[i].conn == conn) {
			guitar_id = i;
			break;
		}
	}
	
	if (guitar_id < 0) {
		LOG_ERR("Notification from unknown connection");
		return BT_GATT_ITER_CONTINUE;
	}
	
	accel = (const struct accel_data *)data;
	process_accel_data(accel, guitar_id);
	
	return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_subscribe_params subscribe_params[MAX_GUITARS];

static uint8_t discover_accel_char(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   struct bt_gatt_discover_params *params)
{
	int err;
	int guitar_id = -1;
	
	if (!attr) {
		LOG_INF("Discover complete");
		return BT_GATT_ITER_STOP;
	}
	
	/* Find which guitar this is */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (guitar_conns[i].conn == conn) {
			guitar_id = i;
			break;
		}
	}
	
	if (guitar_id < 0) {
		return BT_GATT_ITER_STOP;
	}
	
	LOG_INF("Guitar %d: Found acceleration characteristic", guitar_id);
	
	/* Store handle and subscribe to notifications */
	guitar_conns[guitar_id].accel_handle = bt_gatt_attr_value_handle(attr);
	
	subscribe_params[guitar_id].notify = accel_notify_callback;
	subscribe_params[guitar_id].value = BT_GATT_CCC_NOTIFY;
	subscribe_params[guitar_id].value_handle = guitar_conns[guitar_id].accel_handle;
	subscribe_params[guitar_id].ccc_handle = guitar_conns[guitar_id].accel_handle + 1;
	
	err = bt_gatt_subscribe(conn, &subscribe_params[guitar_id]);
	if (err) {
#if BLE_DEBUG
		LOG_ERR("BLE: Subscribe failed for guitar %d (err %d)", guitar_id, err);
#else
		LOG_ERR("Subscribe failed (err %d)", err);
#endif
	} else {
#if BLE_DEBUG
		LOG_INF("BLE: Subscribed to acceleration notifications (guitar %d)", guitar_id);
#else
		LOG_INF("Subscribed to acceleration notifications");
#endif
		guitar_conns[guitar_id].subscribed = true;
	}
	
	return BT_GATT_ITER_STOP;
}

static struct bt_gatt_discover_params discover_params[MAX_GUITARS];

static bool check_guitar_uuid(struct bt_data *data, void *user_data)
{
	bool *found = user_data;

	if (data->type == BT_DATA_UUID128_ALL) {
		if (data->data_len == 16) {
			if (memcmp(data->data, guitar_service_uuid.val, 16) == 0) {
				*found = true;
			}
		}
	}
	return true;
}

static bool check_guitar_name(struct bt_data *data, void *user_data)
{
	bool *found = user_data;
	const char *expected_name = "GuitarAcc Guitar";
	size_t expected_len = strlen(expected_name);

	if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
		if (data->data_len >= expected_len) {
			if (memcmp(data->data, expected_name, expected_len) == 0) {
				*found = true;
			}
		}
	}
	return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
			 struct net_buf_simple *ad)
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err;
	int slot = -1;
	bool has_uuid = false;
	bool has_name = false;

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	/* Check if device advertises Guitar Service UUID */
	bt_data_parse(ad, check_guitar_uuid, &has_uuid);

	/* Check if device name matches "GuitarAcc Guitar" */
	bt_data_parse(ad, check_guitar_name, &has_name);

	if (!has_uuid || !has_name) {
		return; /* Not a guitar device, ignore */
	}

#if BLE_DEBUG
	LOG_INF("BLE: Guitar found! %s (RSSI %d)", addr_str, rssi);
#else
	LOG_INF("Guitar found: %s (RSSI %d)", addr_str, rssi);
#endif

	/* Find empty connection slot */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (!guitar_conns[i].conn) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		LOG_WRN("Max guitars connected, ignoring device");
		return;
	}

#if BLE_DEBUG
	LOG_INF("BLE: Attempting connection to guitar (slot %d)...", slot);
#endif

	err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("Stop LE scan failed (err %d)", err);
		return;
	}

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &guitar_conns[slot].conn);
	if (err) {
#if BLE_DEBUG
		LOG_ERR("BLE: Connection creation failed (err %d)", err);
#else
		LOG_ERR("Create conn failed (err %d)", err);
#endif
		/* Resume scanning */
		bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int guitar_id = -1;
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
#if BLE_DEBUG
		LOG_ERR("BLE: Connection failed to %s (reason 0x%02x)", addr, conn_err);
#else
		LOG_ERR("Failed to connect to %s (%u)", addr, conn_err);
#endif
		/* Clear connection slot */
		for (int i = 0; i < MAX_GUITARS; i++) {
			if (guitar_conns[i].conn == conn) {
				bt_conn_unref(guitar_conns[i].conn);
				guitar_conns[i].conn = NULL;
				guitar_conns[i].subscribed = false;
				break;
			}
		}
		/* Resume scanning for more guitars */
		bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
		return;
	}

#if BLE_DEBUG
	LOG_INF("BLE: Guitar connected successfully: %s", addr);
#else
	LOG_INF("Guitar connected: %s", addr);
#endif

	/* Find which guitar slot this is */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (guitar_conns[i].conn == conn) {
			guitar_id = i;
			break;
		}
	}

	if (guitar_id >= 0) {
		/* Start GATT service discovery */
		discover_params[guitar_id].uuid = &guitar_accel_char_uuid.uuid;
		discover_params[guitar_id].func = discover_accel_char;
		discover_params[guitar_id].start_handle = 0x0001;
		discover_params[guitar_id].end_handle = 0xFFFF;
		discover_params[guitar_id].type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, &discover_params[guitar_id]);
		if (err) {
#if BLE_DEBUG
			LOG_ERR("BLE: GATT discovery failed for guitar %d (err %d)", guitar_id, err);
#else
			LOG_ERR("Discover failed (err %d)", err);
#endif
		} else {
#if BLE_DEBUG
			LOG_INF("BLE: Starting GATT discovery for guitar %d...", guitar_id);
#else
			LOG_INF("Starting GATT discovery for guitar %d", guitar_id);
#endif
		}
	}

	/* Resume scanning for more guitars if slots available */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (!guitar_conns[i].conn) {
			bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
			break;
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
#if BLE_DEBUG
	LOG_INF("BLE: Guitar disconnected: %s (reason 0x%02x)", addr, reason);
#else
	LOG_INF("Guitar disconnected: %s (reason 0x%02x)", addr, reason);
#endif

	/* Clear connection slot */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (guitar_conns[i].conn == conn) {
			bt_conn_unref(guitar_conns[i].conn);
			guitar_conns[i].conn = NULL;
			guitar_conns[i].subscribed = false;
			guitar_conns[i].accel_handle = 0;
			break;
		}
	}

	/* Resume scanning for replacement */
	bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static int init_midi_uart(void)
{
	midi_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
	if (!device_is_ready(midi_uart)) {
		LOG_ERR("MIDI UART device not ready");
		return -ENODEV;
	}

	/* Set up interrupt-driven UART */
	uart_irq_callback_set(midi_uart, uart_isr);
	
	/* Disable RX interrupt (we only use TX) */
	uart_irq_rx_disable(midi_uart);
	
	LOG_INF("MIDI UART initialized (interrupt-driven)");
	LOG_INF("UART ISR callback registered");
	return 0;
}

#if TEST_MODE_ENABLED
static void send_midi_test_message(void)
{
	/* MIDI Note On message: Status (0x90 = Note On channel 0), Note (60 = Middle C), Velocity (64) */
	uint8_t note_on[] = {0x90, 0x3C, 0x40};
	/* MIDI Note Off message: Status (0x80 = Note Off channel 0), Note (60), Velocity (0) */
	uint8_t note_off[] = {0x80, 0x3C, 0x00};
	
	/* Queue Note On */
	queue_midi_bytes(note_on, sizeof(note_on));
	LOG_INF("Test MIDI: Note ON queued (C4, velocity 64)");
	
	/* Wait a bit, then queue Note Off */
	k_sleep(K_MSEC(100));
	
	queue_midi_bytes(note_off, sizeof(note_off));
	LOG_INF("Test MIDI: Note OFF queued (C4)");
}

static void test_mode_thread(void)
{
	LOG_INF("Test mode enabled - sending MIDI messages every %d ms", TEST_INTERVAL_MS);
	
	while (1) {
		k_sleep(K_MSEC(TEST_INTERVAL_MS));
		send_midi_test_message();
	}
}

K_THREAD_DEFINE(test_thread, 1024, test_mode_thread, NULL, NULL, NULL, 7, 0, 0);
#endif

int main(void)
{
	int err;

	LOG_INF("GuitarAcc Basestation (Central) starting...");

	/* Initialize MIDI UART */
	err = init_midi_uart();
	if (err) {
		LOG_ERR("MIDI UART init failed");
	}

	LOG_INF("Starting Bluetooth initialization...");

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

#if BLE_DEBUG
	LOG_INF("BLE: Bluetooth stack initialized successfully");
#else
	LOG_INF("Bluetooth initialized");
#endif

	LOG_INF("Starting BLE scan...");

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return 0;
	}

#if BLE_DEBUG
	LOG_INF("BLE: Active scanning started, looking for guitar devices...");
#else
	LOG_INF("Scanning for guitars...");
#endif

	LOG_INF("Main initialization complete");

	return 0;
}

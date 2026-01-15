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

LOG_MODULE_REGISTER(basestation, LOG_LEVEL_DBG);

/* Enable test mode to send periodic MIDI test messages */
#define TEST_MODE_ENABLED 1
#define TEST_INTERVAL_MS 1000

/* Set to 1 for testing via VCOM at 115200, 0 for real MIDI at 31250 */
#define USE_VCOM_BAUD 1

#define MAX_GUITARS 4
#if USE_VCOM_BAUD
#define MIDI_BAUD_RATE 115200
#else
#define MIDI_BAUD_RATE 31250
#endif

/* Custom Guitar Service UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f */
#define BT_UUID_GUITAR_SERVICE_VAL \
	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a3f)

static struct bt_uuid_128 guitar_service_uuid = BT_UUID_INIT_128(
	BT_UUID_GUITAR_SERVICE_VAL);

static struct bt_conn *guitar_conns[MAX_GUITARS];
static const struct device *midi_uart;

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

	LOG_INF("Guitar found: %s (RSSI %d)", addr_str, rssi);

	/* Find empty connection slot */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (!guitar_conns[i]) {
			slot = i;
			break;
		}
	}

	if (slot < 0) {
		LOG_WRN("Max guitars connected, ignoring device");
		return;
	}

	err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("Stop LE scan failed (err %d)", err);
		return;
	}

	err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN,
				BT_LE_CONN_PARAM_DEFAULT, &guitar_conns[slot]);
	if (err) {
		LOG_ERR("Create conn failed (err %d)", err);
		/* Resume scanning */
		bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s (%u)", addr, conn_err);
		/* Clear connection slot */
		for (int i = 0; i < MAX_GUITARS; i++) {
			if (guitar_conns[i] == conn) {
				bt_conn_unref(guitar_conns[i]);
				guitar_conns[i] = NULL;
				break;
			}
		}
		/* Resume scanning for more guitars */
		bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
		return;
	}

	LOG_INF("Guitar connected: %s", addr);

	/* Resume scanning for more guitars if slots available */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (!guitar_conns[i]) {
			bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
			break;
		}
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Guitar disconnected: %s (reason 0x%02x)", addr, reason);

	/* Clear connection slot */
	for (int i = 0; i < MAX_GUITARS; i++) {
		if (guitar_conns[i] == conn) {
			bt_conn_unref(guitar_conns[i]);
			guitar_conns[i] = NULL;
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
	const struct uart_config midi_cfg = {
		.baudrate = MIDI_BAUD_RATE,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	midi_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
	if (!device_is_ready(midi_uart)) {
		LOG_ERR("MIDI UART device not ready");
		return -ENODEV;
	}

	int err = uart_configure(midi_uart, &midi_cfg);
	if (err) {
		LOG_ERR("UART configure failed (err %d)", err);
		return err;
	}

	LOG_INF("MIDI UART initialized at %d baud", MIDI_BAUD_RATE);
	return 0;
}

#if TEST_MODE_ENABLED
static void send_midi_test_message(void)
{
	/* MIDI Note On message: Status (0x90 = Note On channel 0), Note (60 = Middle C), Velocity (64) */
	uint8_t note_on[] = {0x90, 0x3C, 0x40};
	/* MIDI Note Off message: Status (0x80 = Note Off channel 0), Note (60), Velocity (0) */
	uint8_t note_off[] = {0x80, 0x3C, 0x00};
	
	if (!midi_uart) {
		return;
	}

	/* Send Note On */
	for (int i = 0; i < sizeof(note_on); i++) {
		uart_poll_out(midi_uart, note_on[i]);
	}
	
	LOG_INF("Test MIDI: Note ON sent (C4, velocity 64)");
	
	/* Wait a bit, then send Note Off */
	k_sleep(K_MSEC(100));
	
	for (int i = 0; i < sizeof(note_off); i++) {
		uart_poll_out(midi_uart, note_off[i]);
	}
	
	LOG_INF("Test MIDI: Note OFF sent (C4)");
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

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");

	err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)", err);
		return 0;
	}

	LOG_INF("Scanning for guitars...");

	return 0;
}

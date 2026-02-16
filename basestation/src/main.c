/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/scan.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/services/hogp.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/settings/settings.h>
#include <zephyr/drivers/uart.h>
#include "midi_logic.h"
#include "ui_led.h"

LOG_MODULE_REGISTER(basestation, LOG_LEVEL_DBG);

/* Debug flags */
#define MIDI_DEBUG 0  /* Enable detailed MIDI transmission logging */
#define BLE_DEBUG 1   /* Enable BLE connection and discovery logging */

/* Enable test mode to send periodic MIDI test messages */
//#define TEST_MODE_ENABLED 1
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

/* MIDI UART device */
static const struct device *midi_uart;

/* MIDI TX queue for interrupt-driven transmission */
#define MIDI_TX_QUEUE_SIZE 16
#define MIDI_TX_MAX_QUEUED 6  /* Don't write if more than this many bytes queued */
static uint8_t midi_tx_queue[MIDI_TX_QUEUE_SIZE];
static volatile size_t midi_tx_head = 0;
static volatile size_t midi_tx_tail = 0;
static K_SEM_DEFINE(midi_tx_sem, 0, 1);

/* Guitar connection state */
struct guitar_connection {
	struct bt_conn *conn;
	uint16_t accel_handle;
	bool subscribed;
};

static struct guitar_connection guitar_conn = {0};

/* Accelerometer to MIDI mapping configurations */
static struct accel_mapping_config x_axis_config;
static struct accel_mapping_config y_axis_config;
static struct accel_mapping_config z_axis_config;

/* UART ISR for interrupt-driven MIDI transmission */
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
	
	/* Drain RX FIFO if needed */
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
	
	/* Calculate current queue depth */
	size_t queued = (midi_tx_head >= midi_tx_tail) ? 
		(midi_tx_head - midi_tx_tail) : 
		(MIDI_TX_QUEUE_SIZE - midi_tx_tail + midi_tx_head);
	
	/* Check if queue is too full - reject entire write if so */
	if (queued > MIDI_TX_MAX_QUEUED) {
		LOG_WRN("MIDI TX queue too full (%d bytes), dropping message", queued);
		return -ENOMEM;
	}
	
	/* Check if there's enough space for the entire message */
	size_t available = (MIDI_TX_QUEUE_SIZE - 1) - queued;
	if (len > available) {
		LOG_WRN("Not enough space in MIDI TX queue (%d available, %d needed), dropping message", 
			available, len);
		return -ENOMEM;
	}
	
	/* Add bytes to queue */
	for (size_t i = 0; i < len; i++) {
		midi_tx_queue[midi_tx_head] = data[i];
		midi_tx_head = (midi_tx_head + 1) % MIDI_TX_QUEUE_SIZE;
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

/* Process acceleration data and convert to MIDI CC */
static void process_accel_data(const struct accel_data *accel, int guitar_id)
{
	uint8_t cc_x, cc_y, cc_z;
	
	/* Convert acceleration to MIDI CC values using custom mappings */
	/* X: -1027mg to -431mg, Y: -796mg to 111mg, Z: 61mg to -827mg (inverted) */
	cc_x = accel_to_midi_cc(accel->x, &x_axis_config);
	cc_y = accel_to_midi_cc(accel->y, &y_axis_config);
	cc_z = accel_to_midi_cc(accel->z, &z_axis_config);
	
	/* Send MIDI CC messages on channel 1 */
	send_midi_cc(0, MIDI_CC_X_AXIS, cc_x);  /* Channel 1, CC 16 */
	send_midi_cc(0, MIDI_CC_Y_AXIS, cc_y);  /* Channel 1, CC 17 */
	send_midi_cc(0, MIDI_CC_Z_AXIS, cc_z);  /* Channel 1, CC 18 */
	
	/* Brief LED flash to indicate MIDI activity */
	ui_led_flash(UI_LED_WHITE, 30);  /* 30ms white flash */
	
#if BLE_DEBUG
	LOG_INF("Accel: x=%d y=%d z=%d -> MIDI: %d %d %d", 
		accel->x, accel->y, accel->z, cc_x, cc_y, cc_z);
#endif
}

/**
 * Switch between boot protocol and report protocol mode.
 */
#define KEY_BOOTMODE_MASK DK_BTN2_MSK
/**
 * Switch CAPSLOCK state.
 *
 * @note
 * For simplicity of the code it works only in boot mode.
 */
#define KEY_CAPSLOCK_MASK DK_BTN1_MSK
/**
 * Switch CAPSLOCK state with response
 *
 * Write CAPSLOCK with response.
 * Just for testing purposes.
 * The result should be the same like usine @ref KEY_CAPSLOCK_MASK
 */
#define KEY_CAPSLOCK_RSP_MASK DK_BTN3_MSK

/* Key used to accept or reject passkey value */
#define KEY_PAIRING_ACCEPT DK_BTN1_MSK
#define KEY_PAIRING_REJECT DK_BTN2_MSK

static struct bt_conn *default_conn;
static struct bt_hogp hogp;
static struct bt_conn *auth_conn;
static uint8_t capslock_state;

static void hids_on_ready(struct k_work *work);
static K_WORK_DEFINE(hids_ready_work, hids_on_ready);


static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!filter_match->uuid.match ||
	    (filter_match->uuid.count != 1)) {

		printk("Invalid device connected\n");

		return;
	}

	const struct bt_uuid *uuid = filter_match->uuid.uuid[0];

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));

	printk("Filters matched on UUID 0x%04x.\nAddress: %s connectable: %s\n",
		BT_UUID_16(uuid)->val,
		addr, connectable ? "yes" : "no");
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	printk("Connecting failed\n");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	default_conn = bt_conn_ref(conn);
}
/** .. include_startingpoint_scan_rst */
static void scan_filter_no_match(struct bt_scan_device_info *device_info,
				 bool connectable)
{
	int err;
	struct bt_conn *conn = NULL;
	char addr[BT_ADDR_LE_STR_LEN];

	if (device_info->recv_info->adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_addr_le_to_str(device_info->recv_info->addr, addr,
				  sizeof(addr));
		printk("Direct advertising received from %s\n", addr);
		bt_scan_stop();

		err = bt_conn_le_create(device_info->recv_info->addr,
					BT_CONN_LE_CREATE_CONN,
					device_info->conn_param, &conn);

		if (!err) {
			default_conn = bt_conn_ref(conn);
			bt_conn_unref(conn);
		}
	}
}
/** .. include_endpoint_scan_rst */
BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match,
		scan_connecting_error, scan_connecting);

/* Guitar acceleration notification callback */
static uint8_t accel_notify_callback(struct bt_conn *conn,
				     struct bt_gatt_subscribe_params *params,
				     const void *data, uint16_t length)
{
	const struct accel_data *accel;
	
	if (!data) {
		LOG_INF("Unsubscribed from acceleration notifications");
		params->value_handle = 0;
		return BT_GATT_ITER_STOP;
	}
	
#if BLE_DEBUG
	LOG_INF("BLE: Received notification, length=%d", length);
#endif
	
	if (length != sizeof(struct accel_data)) {
		LOG_WRN("Invalid acceleration data length: %d (expected %d)", length, sizeof(struct accel_data));
		return BT_GATT_ITER_CONTINUE;
	}
	
	accel = (const struct accel_data *)data;
	process_accel_data(accel, 0);  /* Single guitar, ID = 0 */
	
	return BT_GATT_ITER_CONTINUE;
}

static struct bt_gatt_subscribe_params subscribe_params;

static struct bt_gatt_discover_params discover_params;

/* Discover acceleration characteristic within guitar service */
static uint8_t discover_accel_char(struct bt_conn *conn,
				   const struct bt_gatt_attr *attr,
				   struct bt_gatt_discover_params *params)
{
	int err;
	
	if (!attr) {
		LOG_INF("Guitar service discovery complete");
		return BT_GATT_ITER_STOP;
	}
	
	LOG_INF("Found acceleration characteristic");
	
	/* Store handle and subscribe to notifications */
	guitar_conn.accel_handle = bt_gatt_attr_value_handle(attr);
	
	memset(&subscribe_params, 0, sizeof(subscribe_params));
	subscribe_params.notify = accel_notify_callback;
	subscribe_params.value = BT_GATT_CCC_NOTIFY;
	subscribe_params.value_handle = guitar_conn.accel_handle;
	subscribe_params.ccc_handle = guitar_conn.accel_handle + 1;  /* CCC is typically next handle */
	
	err = bt_gatt_subscribe(conn, &subscribe_params);
	if (err && err != -EALREADY) {
#if BLE_DEBUG
		LOG_ERR("BLE: Subscribe failed (err %d)", err);
#else
		LOG_ERR("Subscribe failed (err %d)", err);
#endif
	} else {
		guitar_conn.subscribed = true;
#if BLE_DEBUG
		LOG_INF("BLE: Subscribed to acceleration notifications");
#else
		LOG_INF("Subscribed to acceleration notifications");
#endif
	}
	
	return BT_GATT_ITER_STOP;
}

/* Guitar service discovery - replaces HIDS discovery for guitar connection */
static void discover_guitar_service(struct bt_conn *conn)
{
	int err;
	
	LOG_INF("Starting guitar service discovery");
	
	memset(&discover_params, 0, sizeof(discover_params));
	discover_params.uuid = &guitar_accel_char_uuid.uuid;
	discover_params.func = discover_accel_char;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	
	err = bt_gatt_discover(conn, &discover_params);
	if (err) {
		LOG_ERR("Guitar service discovery failed (err %d)", err);
	}
}

static void discovery_completed_cb(struct bt_gatt_dm *dm,
				   void *context)
{
	int err;

	printk("The discovery procedure succeeded\n");

	bt_gatt_dm_data_print(dm);

	err = bt_hogp_handles_assign(dm, &hogp);
	if (err) {
		printk("Could not init HIDS client object, error: %d\n", err);
	}

	err = bt_gatt_dm_data_release(dm);
	if (err) {
		printk("Could not release the discovery data, error "
		       "code: %d\n", err);
	}
}

static void discovery_service_not_found_cb(struct bt_conn *conn,
					   void *context)
{
	printk("The service could not be found during the discovery\n");
}

static void discovery_error_found_cb(struct bt_conn *conn,
				     int err,
				     void *context)
{
	printk("The discovery procedure failed with %d\n", err);
}

static const struct bt_gatt_dm_cb discovery_cb = {
	.completed = discovery_completed_cb,
	.service_not_found = discovery_service_not_found_cb,
	.error_found = discovery_error_found_cb,
};

static void gatt_discover(struct bt_conn *conn)
{
	int err;

	if (conn != default_conn) {
		return;
	}

	err = bt_gatt_dm_start(conn, BT_UUID_HIDS, &discovery_cb, NULL);
	if (err) {
		printk("could not start the discovery procedure, error "
			"code: %d\n", err);
	}
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s, 0x%02x %s\n", addr, conn_err,
		       bt_hci_err_to_str(conn_err));
		if (conn == default_conn) {
			bt_conn_unref(default_conn);
			default_conn = NULL;

			/* This demo doesn't require active scan */
			err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
			if (err) {
				printk("Scanning failed to start (err %d)\n", err);
			}
		}
		return;
	}

	printk("Connected: %s\n", addr);
	
	/* Track guitar connection */
	guitar_conn.conn = bt_conn_ref(conn);
	guitar_conn.subscribed = false;
	
	/* Update LED to show connected state */
	ui_led_update_connection_count(1);
	
#if BLE_DEBUG
	LOG_INF("BLE: Guitar connected");
#endif

	err = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (err) {
		printk("Failed to set security: %d\n", err);
		/* Discover guitar service instead of HIDS */
		discover_guitar_service(conn);
		/* Keep HIDS discovery for compatibility */
		gatt_discover(conn);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	printk("Disconnected: %s, reason 0x%02x %s\n", addr, reason, bt_hci_err_to_str(reason));

	/* Clean up guitar connection */
	if (guitar_conn.conn == conn) {
			bt_conn_unref(guitar_conn.conn);
		guitar_conn.conn = NULL;
		guitar_conn.subscribed = false;
		
		/* Update LED to show disconnected/scanning state */
		ui_led_update_connection_count(0);
		
#if BLE_DEBUG
		LOG_INF("BLE: Guitar disconnected");
#endif
	}

	if (bt_hogp_assign_check(&hogp)) {
		printk("HIDS client active - releasing");
		bt_hogp_release(&hogp);
	}

	if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	/* This demo doesn't require active scan */
	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
	}
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr, level);
	} else {
		printk("Security failed: %s level %u err %d %s\n", addr, level, err,
		       bt_security_err_to_str(err));
	}

	/* Discover guitar service in addition to HIDS */
	discover_guitar_service(conn);
	gatt_discover(conn);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected        = connected,
	.disconnected     = disconnected,
	.security_changed = security_changed
};

static void scan_init(void)
{
	int err;

	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
		.scan_param = NULL,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &guitar_service_uuid);
	if (err) {
		printk("Scanning filters cannot be set (err %d)\n", err);

		return;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		printk("Filters cannot be turned on (err %d)\n", err);
	}
}

static uint8_t hogp_notify_cb(struct bt_hogp *hogp,
			     struct bt_hogp_rep_info *rep,
			     uint8_t err,
			     const uint8_t *data)
{
	uint8_t size = bt_hogp_rep_size(rep);
	uint8_t i;

	if (!data) {
		return BT_GATT_ITER_STOP;
	}
	printk("Notification, id: %u, size: %u, data:",
	       bt_hogp_rep_id(rep),
	       size);
	for (i = 0; i < size; ++i) {
		printk(" 0x%x", data[i]);
	}
	printk("\n");
	return BT_GATT_ITER_CONTINUE;
}

static uint8_t hogp_boot_mouse_report(struct bt_hogp *hogp,
				     struct bt_hogp_rep_info *rep,
				     uint8_t err,
				     const uint8_t *data) {
	uint8_t size = bt_hogp_rep_size(rep);
	uint8_t i;

	if (!data) {
		return BT_GATT_ITER_STOP;
		}
	printk("Notification, mouse boot, size: %u, data:", size);
	for (i = 0; i < size; ++i) {
		printk(" 0x%x", data[i]);
	}
	printk("\n");
		return BT_GATT_ITER_CONTINUE;
	}

static uint8_t hogp_boot_kbd_report(struct bt_hogp *hogp,
				   struct bt_hogp_rep_info *rep,
				   uint8_t err,
				   const uint8_t *data)
{
	uint8_t size = bt_hogp_rep_size(rep);
	uint8_t i;

	if (!data) {
		return BT_GATT_ITER_STOP;
	}
	printk("Notification, keyboard boot, size: %u, data:", size);
	for (i = 0; i < size; ++i) {
		printk(" 0x%x", data[i]);
	}
	printk("\n");
	return BT_GATT_ITER_CONTINUE;
}

static void hogp_ready_cb(struct bt_hogp *hogp)
{
	k_work_submit(&hids_ready_work);
}

static void hids_on_ready(struct k_work *work)
{
	int err;
	struct bt_hogp_rep_info *rep = NULL;

	printk("HIDS is ready to work\n");

	while (NULL != (rep = bt_hogp_rep_next(&hogp, rep))) {
		if (bt_hogp_rep_type(rep) ==
		    BT_HIDS_REPORT_TYPE_INPUT) {
			printk("Subscribe to report id: %u\n",
			       bt_hogp_rep_id(rep));
			err = bt_hogp_rep_subscribe(&hogp, rep,
							   hogp_notify_cb);
	if (err) {
				printk("Subscribe error (%d)\n", err);
			}
		}
	}
	if (hogp.rep_boot.kbd_inp) {
		printk("Subscribe to boot keyboard report\n");
		err = bt_hogp_rep_subscribe(&hogp,
						   hogp.rep_boot.kbd_inp,
						   hogp_boot_kbd_report);
		if (err) {
			printk("Subscribe error (%d)\n", err);
		}
	}
	if (hogp.rep_boot.mouse_inp) {
		printk("Subscribe to boot mouse report\n");
		err = bt_hogp_rep_subscribe(&hogp,
						   hogp.rep_boot.mouse_inp,
						   hogp_boot_mouse_report);
		if (err) {
			printk("Subscribe error (%d)\n", err);
		}
}
}

static void hogp_prep_fail_cb(struct bt_hogp *hogp, int err)
{
	printk("ERROR: HIDS client preparation failed!\n");
}

static void hogp_pm_update_cb(struct bt_hogp *hogp)
{
	printk("Protocol mode updated: %s\n",
	      bt_hogp_pm_get(hogp) == BT_HIDS_PM_BOOT ?
	      "BOOT" : "REPORT");
}

/* HIDS client initialization parameters */
static const struct bt_hogp_init_params hogp_init_params = {
	.ready_cb      = hogp_ready_cb,
	.prep_error_cb = hogp_prep_fail_cb,
	.pm_update_cb  = hogp_pm_update_cb
};

static void button_bootmode(void)
{
	if (!bt_hogp_ready_check(&hogp)) {
		printk("HID device not ready\n");
		return;
	}
	int err;
	enum bt_hids_pm pm = bt_hogp_pm_get(&hogp);
	enum bt_hids_pm new_pm = ((pm == BT_HIDS_PM_BOOT) ? BT_HIDS_PM_REPORT : BT_HIDS_PM_BOOT);

	printk("Setting protocol mode: %s\n", (new_pm == BT_HIDS_PM_BOOT) ? "BOOT" : "REPORT");
	err = bt_hogp_pm_write(&hogp, new_pm);
	if (err) {
		printk("Cannot change protocol mode (err %d)\n", err);
	}
}

static void hidc_write_cb(struct bt_hogp *hidc,
			  struct bt_hogp_rep_info *rep,
			  uint8_t err)
{
	printk("Caps lock sent\n");
}

static void button_capslock(void)
{
	int err;
	uint8_t data;

	if (!bt_hogp_ready_check(&hogp)) {
		printk("HID device not ready\n");
		return;
	}
	if (!hogp.rep_boot.kbd_out) {
		printk("HID device does not have Keyboard OUT report\n");
		return;
	}
	if (bt_hogp_pm_get(&hogp) != BT_HIDS_PM_BOOT) {
		printk("This function works only in BOOT Report mode\n");
		return;
	}
	capslock_state = capslock_state ? 0 : 1;
	data = capslock_state ? 0x02 : 0;
	err = bt_hogp_rep_write_wo_rsp(&hogp, hogp.rep_boot.kbd_out,
				       &data, sizeof(data),
				       hidc_write_cb);

	if (err) {
		printk("Keyboard data write error (err: %d)\n", err);
		return;
	}
	printk("Caps lock send (val: 0x%x)\n", data);
}


static uint8_t capslock_read_cb(struct bt_hogp *hogp,
			     struct bt_hogp_rep_info *rep,
			     uint8_t err,
			     const uint8_t *data)
{
	if (err) {
		printk("Capslock read error (err: %u)\n", err);
		return BT_GATT_ITER_STOP;
	}
	if (!data) {
		printk("Capslock read - no data\n");
		return BT_GATT_ITER_STOP;
	}
	printk("Received data (size: %u, data[0]: 0x%x)\n",
	       bt_hogp_rep_size(rep), data[0]);

	return BT_GATT_ITER_STOP;
}

static void capslock_write_cb(struct bt_hogp *hogp,
			      struct bt_hogp_rep_info *rep,
			      uint8_t err)
{
	int ret;

	printk("Capslock write result: %u\n", err);

	ret = bt_hogp_rep_read(hogp, rep, capslock_read_cb);
	if (ret) {
		printk("Cannot read capslock value (err: %d)\n", ret);
	}
}


static void button_capslock_rsp(void) {
	if (!bt_hogp_ready_check(&hogp)) {
		printk("HID device not ready\n");
		return;
	}
	if (!hogp.rep_boot.kbd_out) {
		printk("HID device does not have Keyboard OUT report\n");
		return;
	}
	int err;
	uint8_t data;

	capslock_state = capslock_state ? 0 : 1;
	data = capslock_state ? 0x02 : 0;
	err = bt_hogp_rep_write(&hogp, hogp.rep_boot.kbd_out, capslock_write_cb,
				&data, sizeof(data));
	if (err) {
		printk("Keyboard data write error (err: %d)\n", err);
		return;
	}
	printk("Caps lock send using write with response (val: 0x%x)\n", data);
}


static void num_comp_reply(bool accept)
{
	if (accept) {
		bt_conn_auth_passkey_confirm(auth_conn);
		printk("Numeric Match, conn %p\n", auth_conn);
	} else {
		bt_conn_auth_cancel(auth_conn);
		printk("Numeric Reject, conn %p\n", auth_conn);
	}

	bt_conn_unref(auth_conn);
	auth_conn = NULL;
}


static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	uint32_t button = button_state & has_changed;

	if (auth_conn) {
		if (button & KEY_PAIRING_ACCEPT) {
			num_comp_reply(true);
		}

		if (button & KEY_PAIRING_REJECT) {
			num_comp_reply(false);
		}

		return;
	}

	if (button & KEY_BOOTMODE_MASK) {
		button_bootmode();
	}
	if (button & KEY_CAPSLOCK_MASK) {
		button_capslock();
	}
	if (button & KEY_CAPSLOCK_RSP_MASK) {
		button_capslock_rsp();
	}
}


static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
}


static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	auth_conn = bt_conn_ref(conn);

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Passkey for %s: %06u\n", addr, passkey);
	if (IS_ENABLED(CONFIG_SOC_SERIES_NRF54HX) || IS_ENABLED(CONFIG_SOC_SERIES_NRF54LX)) {
		printk("Press Button 0 to confirm, Button 1 to reject.\n");
	} else {
		printk("Press Button 1 to confirm, Button 2 to reject.\n");
	}
}


static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);
	}


static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing completed: %s, bonded: %d\n", addr, bonded);
}


static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed conn: %s, reason %d %s\n", addr, reason,
	       bt_security_err_to_str(reason));
	}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};


int main(void)
{
	int err;

	printk("Starting Bluetooth Central HIDS sample\n");

	/* Initialize UI LED subsystem */
	err = ui_led_init();
	if (err) {
		printk("Failed to initialize UI LED (err %d)\n", err);
		/* Continue anyway - LED is not critical */
	}

	/* Initialize MIDI UART */
	midi_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
	if (!device_is_ready(midi_uart)) {
		LOG_ERR("MIDI UART device not ready");
		return 0;
	}
	
	/* Set up interrupt-driven UART */
	uart_irq_callback_set(midi_uart, uart_isr);
	
	/* Disable RX interrupt (we only use TX) */
	uart_irq_rx_disable(midi_uart);
	
	LOG_INF("MIDI UART initialized (interrupt-driven)");

	/* Initialize accelerometer to MIDI mapping configurations */
	/* Based on captured data: X[837:935], Y[56:294], Z[223:665] */
	accel_mapping_init_linear(&x_axis_config, 837, 935);
	accel_mapping_init_linear(&y_axis_config, 56, 294);
	accel_mapping_init_linear(&z_axis_config, 223, 665);
	LOG_INF("Accel mapping: X[837:935] Y[56:294] Z[223:665] -> MIDI[0:127]");

	bt_hogp_init(&hogp, &hogp_init_params);

	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		printk("failed to register authorization callbacks.\n");
		return 0;
	}

	err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	if (err) {
		printk("Failed to register authorization info callbacks.\n");
		return 0;
	}

	printk("DEBUG: About to call bt_enable(NULL)\n");

	err = bt_enable(NULL);
	printk("DEBUG: bt_enable returned with err=%d\n", err);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	printk("Bluetooth initialized\n");

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	scan_init();

	err = dk_buttons_init(button_handler);
	if (err) {
		printk("Failed to initialize buttons (err %d)\n", err);
		return 0;
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		ui_led_set_state(UI_STATE_ERROR);
		return 0;
	}

	printk("Scanning successfully started\n");
	
	/* Set LED to scanning state */
	ui_led_set_state(UI_STATE_SCANNING);

	return 0;
}

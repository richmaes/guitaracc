/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include <soc.h>
#include <assert.h>

#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <dk_buttons_and_leds.h>
// COMMENTED OUT FOR TROUBLESHOOTING
// #include <zephyr/device.h>
// #include <zephyr/drivers/sensor.h>
// #include <zephyr/pm/device.h>
// #include <math.h>
// #include "motion_logic.h"

LOG_MODULE_REGISTER(guitar, LOG_LEVEL_DBG);

/* ========== DEFINES ========== */

/* Enable test mode to generate incrementing accelerometer data */
//#define TEST_MODE_ENABLED 1

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED          DK_LED1
#define CON_STATUS_LED          DK_LED2

// COMMENTED OUT FOR TROUBLESHOOTING
// #define ACCEL_ALIAS DT_ALIAS(accel0)
// #define MOTION_TIMEOUT_MS 30000  /* 30 seconds of inactivity before sleep */
//
// /* Custom Guitar Service UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f */
// #define BT_UUID_GUITAR_SERVICE_VAL \
// 	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a3f)
//
// /* Acceleration Data Characteristic UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a40 */
// #define BT_UUID_GUITAR_ACCEL_CHAR_VAL \
// 	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a40)

/* ========== STATIC VARIABLES ========== */

// COMMENTED OUT FOR TROUBLESHOOTING
// #if !TEST_MODE_ENABLED
// static const struct device *accel_dev = DEVICE_DT_GET(ACCEL_ALIAS);
// #endif
//
// static struct k_timer motion_timer;
// static bool is_sleeping = false;
static bool is_connected = false;
//
// #if TEST_MODE_ENABLED
// static int16_t test_counter = 0;
// #endif
//
// static struct bt_uuid_128 guitar_service_uuid = BT_UUID_INIT_128(
// 	BT_UUID_GUITAR_SERVICE_VAL);
//
// static struct bt_uuid_128 guitar_accel_char_uuid = BT_UUID_INIT_128(
// 	BT_UUID_GUITAR_ACCEL_CHAR_VAL);
//
// static struct accel_data current_accel;
// static struct accel_data previous_accel;
// static bool accel_notify_enabled = false;

/* ========== ADVERTISING DATA ========== */
#if CONFIG_BT_DIRECTED_ADVERTISING
/* Bonded address queue. */
K_MSGQ_DEFINE(bonds_queue,
	      sizeof(bt_addr_le_t),
	      CONFIG_BT_MAX_PAIRED,
	      4);
#endif

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	// COMMENTED OUT FOR TROUBLESHOOTING
	// BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
	//	      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
	//	      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	// BT_DATA(BT_DATA_UUID128_ALL, guitar_service_uuid.val, sizeof(guitar_service_uuid.val)),
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/* ========== HELPER FUNCTIONS ========== */

// COMMENTED OUT FOR TROUBLESHOOTING
// static void motion_timeout_handler(struct k_timer *timer)
// {
// 	if (!is_connected) {
// 		LOG_INF("No motion detected, entering sleep mode");
// 		is_sleeping = true;
// 		bt_le_adv_stop();
// #if !TEST_MODE_ENABLED
// 		/* Put accelerometer in motion detection mode with lower power */
// 		pm_device_action_run(accel_dev, PM_DEVICE_ACTION_SUSPEND);
// #endif
// 	}
// }
//
// static void wake_from_motion(void)
// {
// 	if (is_sleeping) {
// 		LOG_INF("Motion detected, waking up");
// 		is_sleeping = false;
// #if !TEST_MODE_ENABLED
// 		/* Wake up accelerometer */
// 		pm_device_action_run(accel_dev, PM_DEVICE_ACTION_RESUME);
// #endif
// 		/* Restart advertising */
// 		bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), NULL, 0);
// 	}
// 	/* Reset inactivity timer */
// 	k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);
// }

/* ========== CONNECTION CALLBACKS ========== */

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected");
		is_connected = true;
		dk_set_led_on(CON_STATUS_LED);
		// COMMENTED OUT FOR TROUBLESHOOTING
		// k_timer_stop(&motion_timer);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);
	is_connected = false;
	dk_set_led_off(CON_STATUS_LED);
	// COMMENTED OUT FOR TROUBLESHOOTING
	// accel_notify_enabled = false;
	// k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);
}

static void recycled_cb(void)
{
	LOG_INF("Connection recycled, restarting advertising");
	/* Restart advertising after connection object is recycled */
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
	}
}

#ifdef CONFIG_BT_SMP
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_ERR("Security failed: %s level %u err %d", addr, level, err);
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.recycled = recycled_cb,
#ifdef CONFIG_BT_SMP
	.security_changed = security_changed,
#endif
};

/* ========== AUTHENTICATION CALLBACKS ========== */

#if defined(CONFIG_BT_SMP)
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_ERR("Pairing failed conn: %s, reason %d", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {
	.passkey_display = auth_passkey_display,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

/* ========== GATT CALLBACKS ========== */

// COMMENTED OUT FOR TROUBLESHOOTING
// static void accel_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
// {
// 	accel_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
// 	LOG_INF("Acceleration notifications %s", accel_notify_enabled ? "enabled" : "disabled");
// }
//
// /* ========== GATT SERVICE DEFINITION ========== */
//
// /* Guitar GATT Service Definition */
// BT_GATT_SERVICE_DEFINE(guitar_svc,
// 	BT_GATT_PRIMARY_SERVICE(&guitar_service_uuid),
// 	BT_GATT_CHARACTERISTIC(&guitar_accel_char_uuid.uuid,
// 			       BT_GATT_CHRC_NOTIFY,
// 			       BT_GATT_PERM_NONE,
// 			       NULL, NULL, NULL),
// 	BT_GATT_CCC(accel_ccc_cfg_changed,
// 		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
// );

/* ========== DATA SENDING FUNCTIONS ========== */

// COMMENTED OUT FOR TROUBLESHOOTING
// static int send_accel_notification(struct bt_conn *conn)
// {
// 	int err;
// 	
// 	if (!accel_notify_enabled) {
// 		return 0;
// 	}
//
// 	/* Only send if data has changed (power optimization) */
// 	if (!accel_data_changed(&current_accel, &previous_accel)) {
// 		return 0;
// 	}
//
// 	err = bt_gatt_notify(conn, &guitar_svc.attrs[1], &current_accel, sizeof(current_accel));
// 	if (err) {
// 		LOG_ERR("Failed to send notification (err %d)", err);
// 		return err;
// 	}
//
// 	/* Update previous values after successful send */
// 	previous_accel = current_accel;
// 	
// 	LOG_DBG("Sent accel: X=%d, Y=%d, Z=%d milli-g", 
// 		current_accel.x, current_accel.y, current_accel.z);
//
// 	return 0;
// }

/* ========== MAIN FUNCTION ========== */

int main(void)
{
	int err;
	// COMMENTED OUT FOR TROUBLESHOOTING
	// struct sensor_value accel[3];

	LOG_INF("BLE Peripheral starting (simplified for troubleshooting)...");

	// COMMENTED OUT FOR TROUBLESHOOTING
// #if TEST_MODE_ENABLED
// 	LOG_INF("Running in TEST MODE - no accelerometer required");
// #else
// 	/* Initialize accelerometer */
// 	if (!device_is_ready(accel_dev)) {
// 		LOG_ERR("Accelerometer device not ready");
// 		return 0;
// 	}
// 	LOG_INF("Accelerometer initialized");
// #endif
//
// 	/* Initialize motion timer */
// 	k_timer_init(&motion_timer, motion_timeout_handler, NULL);

	/* Register authentication callbacks */
	err = dk_leds_init();
	if (err) {
		LOG_ERR("LEDs init failed (err %d)", err);
		return 0;
	}

	if (IS_ENABLED(CONFIG_BT_SMP)) {
		err = bt_conn_auth_cb_register(&conn_auth_callbacks);
		if (err) {
			LOG_ERR("Failed to register authorization callbacks (err %d)", err);
			return 0;
		}

		err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
		if (err) {
			LOG_ERR("Failed to register authorization info callbacks (err %d)", err);
			return 0;
		}
	}

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");

	/* Load settings from persistent storage */
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return 0;
	}

	LOG_INF("Advertising successfully started, device name: %s", DEVICE_NAME);

	// COMMENTED OUT FOR TROUBLESHOOTING
	// k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);

	/* Main loop: Simple idle loop for BLE peripheral */
	LOG_INF("Entering main loop...");
	while (1) {
		k_sleep(K_SECONDS(5));
		LOG_DBG("Still alive, connected: %s", is_connected ? "yes" : "no");
	}

	return 0;
}

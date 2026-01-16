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
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/pm/device.h>
#include <math.h>

LOG_MODULE_REGISTER(guitar, LOG_LEVEL_DBG);

#define ACCEL_ALIAS DT_ALIAS(accel0)
#define MOTION_TIMEOUT_MS 30000  /* 30 seconds of inactivity before sleep */
#define MOTION_THRESHOLD 0.5     /* m/s² threshold for motion detection */

static const struct device *accel_dev = DEVICE_DT_GET(ACCEL_ALIAS);
static struct k_timer motion_timer;
static bool is_sleeping = false;
static bool is_connected = false;

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

/* Acceleration data structure: X, Y, Z in milli-g (int16_t, 2 bytes each = 6 bytes total) */
struct accel_data {
	int16_t x;  /* X-axis in milli-g */
	int16_t y;  /* Y-axis in milli-g */
	int16_t z;  /* Z-axis in milli-g */
} __packed;

static struct accel_data current_accel;
static bool accel_notify_enabled = false;

static void accel_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	accel_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Acceleration notifications %s", accel_notify_enabled ? "enabled" : "disabled");
}

/* Guitar GATT Service Definition */
BT_GATT_SERVICE_DEFINE(guitar_svc,
	BT_GATT_PRIMARY_SERVICE(&guitar_service_uuid),
	BT_GATT_CHARACTERISTIC(&guitar_accel_char_uuid.uuid,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(accel_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_UUID128_ALL, guitar_service_uuid.val, sizeof(guitar_service_uuid.val)),
};

static void motion_timeout_handler(struct k_timer *timer)
{
	if (!is_connected) {
		LOG_INF("No motion detected, entering sleep mode");
		is_sleeping = true;
		bt_le_adv_stop();
		/* Put accelerometer in motion detection mode with lower power */
		pm_device_action_run(accel_dev, PM_DEVICE_ACTION_SUSPEND);
	}
}

static void wake_from_motion(void)
{
	if (is_sleeping) {
		LOG_INF("Motion detected, waking up");
		is_sleeping = false;
		/* Wake up accelerometer */
		pm_device_action_run(accel_dev, PM_DEVICE_ACTION_RESUME);
		/* Restart advertising */
		bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	}
	/* Reset inactivity timer */
	k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected to basestation");
		is_connected = true;
		k_timer_stop(&motion_timer);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected from basestation (reason 0x%02x)", reason);
	is_connected = false;
	accel_notify_enabled = false;
	/* Restart motion monitoring */
	k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static int send_accel_notification(struct bt_conn *conn)
{
	int err;
	
	if (!accel_notify_enabled) {
		return 0;
	}

	err = bt_gatt_notify(conn, &guitar_svc.attrs[1], &current_accel, sizeof(current_accel));
	if (err) {
		LOG_ERR("Failed to send notification (err %d)", err);
		return err;
	}

	return 0;
}

int main(void)
{
	int err;
	struct sensor_value accel[3];

	LOG_INF("GuitarAcc Guitar (Peripheral) starting...");

	/* Initialize accelerometer */
	if (!device_is_ready(accel_dev)) {
		LOG_ERR("Accelerometer device not ready");
		return 0;
	}
	LOG_INF("Accelerometer initialized");

	/* Initialize motion timer */
	k_timer_init(&motion_timer, motion_timeout_handler, NULL);

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return 0;
	}

	LOG_INF("Bluetooth initialized");

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return 0;
	}

	LOG_INF("Advertising as guitar, waiting for basestation...");

	/* Start motion monitoring */
	k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);

	/* Main loop: monitor accelerometer for motion */
	while (1) {
		if (!is_sleeping) {
			err = sensor_sample_fetch(accel_dev);
			if (err == 0) {
				sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
				
				/* Calculate magnitude of acceleration */
				double x = sensor_value_to_double(&accel[0]);
				double y = sensor_value_to_double(&accel[1]);
				double z = sensor_value_to_double(&accel[2]);
				double magnitude = sqrt(x*x + y*y + z*z);
				
				/* Convert to milli-g (1g = 9.81 m/s²) and store */
				current_accel.x = (int16_t)((x / 9.81) * 1000.0);
				current_accel.y = (int16_t)((y / 9.81) * 1000.0);
				current_accel.z = (int16_t)((z / 9.81) * 1000.0);
				
				/* Send notification if connected */
				if (is_connected) {
					struct bt_conn *conn = NULL;
					bt_conn_foreach(BT_CONN_TYPE_LE, 
						       (void (*)(struct bt_conn *, void *))send_accel_notification,
						       NULL);
				}
				
				/* Detect motion (deviation from 1g gravity) */
				if (fabs(magnitude - 9.81) > MOTION_THRESHOLD) {
					/* Reset timer on motion */
					k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);
				}
			}
			k_sleep(K_MSEC(100)); /* Sample at 10Hz */
		} else {
			/* In sleep mode, use interrupt-driven wake */
			err = sensor_sample_fetch(accel_dev);
			if (err == 0) {
				sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
				double x = sensor_value_to_double(&accel[0]);
				double y = sensor_value_to_double(&accel[1]);
				double z = sensor_value_to_double(&accel[2]);
				double magnitude = sqrt(x*x + y*y + z*z);
				
				if (fabs(magnitude - 9.81) > MOTION_THRESHOLD) {
					wake_from_motion();
				}
			}
			k_sleep(K_MSEC(500)); /* Check less frequently in sleep */
		}
	}

	return 0;
}

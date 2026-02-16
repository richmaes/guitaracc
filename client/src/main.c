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
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
// COMMENTED OUT FOR TROUBLESHOOTING
// #include <zephyr/pm/device.h>
#include <math.h>
#include "motion_logic.h"

LOG_MODULE_REGISTER(guitar, LOG_LEVEL_DBG);

/* ========== DEFINES ========== */

/* Enable test mode to generate incrementing accelerometer data */
//#define TEST_MODE_ENABLED 1

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define RUN_STATUS_LED          DK_LED1
#define CON_STATUS_LED          DK_LED2
#define USER_BUTTON             DK_BTN1_MSK

#define MOVEMENT_THRESHOLD_MILLI_G  50  /* Minimum change to transmit (0.05g) */

#define ACCEL_ALIAS DT_ALIAS(accel0)
// COMMENTED OUT FOR TROUBLESHOOTING
// #define MOTION_TIMEOUT_MS 30000  /* 30 seconds of inactivity before sleep */

/* Custom Guitar Service UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a3f */
#define BT_UUID_GUITAR_SERVICE_VAL \
	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a3f)

/* Acceleration Data Characteristic UUID: a7c8f9d2-4b3e-4a1d-9f2c-8e7d6c5b4a40 */
#define BT_UUID_GUITAR_ACCEL_CHAR_VAL \
	BT_UUID_128_ENCODE(0xa7c8f9d2, 0x4b3e, 0x4a1d, 0x9f2c, 0x8e7d6c5b4a40)

/* ========== STATIC VARIABLES ========== */

#if !TEST_MODE_ENABLED
static const struct device *accel_dev = DEVICE_DT_GET(ACCEL_ALIAS);
#endif

// COMMENTED OUT FOR TROUBLESHOOTING
// static struct k_timer motion_timer;
// static bool is_sleeping = false;
static bool is_connected = false;
static bool is_advertising = false;
static bool transmission_enabled = true; /* Button toggles this */

#if !TEST_MODE_ENABLED
/* GPIO interrupt for accelerometer motion detection */
static struct gpio_callback accel_cb_data;
#endif

#if TEST_MODE_ENABLED
static int16_t test_counter = 0;
#endif

static struct bt_uuid_128 guitar_service_uuid = BT_UUID_INIT_128(
	BT_UUID_GUITAR_SERVICE_VAL);

static struct bt_uuid_128 guitar_accel_char_uuid = BT_UUID_INIT_128(
	BT_UUID_GUITAR_ACCEL_CHAR_VAL);

static struct accel_data current_accel;
static struct accel_data previous_accel;
static bool accel_notify_enabled = false;

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
	BT_DATA_BYTES(BT_DATA_GAP_APPEARANCE,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 0) & 0xff,
		      (CONFIG_BT_DEVICE_APPEARANCE >> 8) & 0xff),
	BT_DATA(BT_DATA_UUID128_ALL, guitar_service_uuid.val, sizeof(guitar_service_uuid.val)),
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

/* ========== LED BLINK THREAD ========== */

#define LED_BLINK_INTERVAL_MS 500  /* 1Hz blink: 500ms on, 500ms off */
#define BLUE_LED DK_LED3  /* Blue component for Thingy:53 RGB LED */

static void led_blink_thread_fn(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1);
	ARG_UNUSED(arg2);
	ARG_UNUSED(arg3);

	bool led_state = false;

	while (1) {
		if (is_advertising && !is_connected) {
			/* Blue blink when advertising but not connected */
			led_state = !led_state;
			dk_set_led(BLUE_LED, led_state);
			k_msleep(LED_BLINK_INTERVAL_MS);
		} else if (is_connected && !transmission_enabled) {
			/* Green blink when connected but transmission disabled */
			led_state = !led_state;
			dk_set_led(CON_STATUS_LED, led_state);
			k_msleep(LED_BLINK_INTERVAL_MS);
		} else {
			/* Solid state handled by connection callbacks */
			k_msleep(100);
		}
	}
}

K_THREAD_STACK_DEFINE(led_blink_stack, 512);
static struct k_thread led_blink_thread;

/* ========== BUTTON HANDLER ========== */

static void button_changed(uint32_t button_state, uint32_t has_changed)
{
	if (has_changed & USER_BUTTON) {
		if (button_state & USER_BUTTON) {
			/* Button pressed - toggle transmission */
			transmission_enabled = !transmission_enabled;
			LOG_INF("Button pressed: Transmission %s", 
			        transmission_enabled ? "ENABLED" : "DISABLED");
			
			/* Update LED immediately if connected */
			if (is_connected) {
				if (transmission_enabled) {
					/* Solid green for transmitting */
					dk_set_led_on(CON_STATUS_LED);
				}
				/* Blink thread will handle blinking state */
			}
		}
	}
}

static int init_button(void)
{
	int err = dk_buttons_init(button_changed);
	if (err) {
		LOG_ERR("Cannot init buttons (err: %d)", err);
	}
	return err;
}

/* ========== ACCELEROMETER INTERRUPT ========== */

#if !TEST_MODE_ENABLED
static void accel_gpio_handler(const struct device *dev,
                               struct gpio_callback *cb,
                               uint32_t pins)
{
	LOG_INF("*** MOTION INTERRUPT DETECTED *** (GPIO pins: 0x%08x)", pins);
	/* TODO: When sleep mode is implemented, call wake_from_motion() here */
	// wake_from_motion();
}
#endif

/* ========== CONNECTION CALLBACKS ========== */

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected");
		is_connected = true;
		is_advertising = false;  /* Stop advertising state */
		dk_set_led_off(BLUE_LED);  /* Turn off blue LED */
		/* LED: Solid green if transmitting, blink green if paused */
		if (transmission_enabled) {
			dk_set_led_on(CON_STATUS_LED);
		}
		/* Blink thread handles blinking when transmission_enabled=false */
		// COMMENTED OUT FOR TROUBLESHOOTING
		// k_timer_stop(&motion_timer);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	LOG_INF("Disconnected (reason 0x%02x)", reason);
	is_connected = false;
	dk_set_led_off(CON_STATUS_LED); /* Turn off green LED */
	dk_set_led_off(BLUE_LED); /* Turn off blue LED */
	accel_notify_enabled = false;
	// COMMENTED OUT FOR TROUBLESHOOTING
	// k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);
}

static void recycled_cb(void)
{
	LOG_INF("Connection recycled, restarting advertising");
	/* Restart advertising after connection object is recycled */
	int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
	} else {
		is_advertising = true;
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

static void accel_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	accel_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_INF("Acceleration notifications %s", accel_notify_enabled ? "enabled" : "disabled");
}

/* ========== GATT SERVICE DEFINITION ========== */

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

/* ========== DATA SENDING FUNCTIONS ========== */

static int send_accel_notification(struct bt_conn *conn)
{
	int err;
	
	if (!accel_notify_enabled || !transmission_enabled) {
		return 0;
	}

	/* Only send if movement exceeds threshold (filter minor changes/noise) */
	if (!detect_movement_threshold(&current_accel, &previous_accel, MOVEMENT_THRESHOLD_MILLI_G)) {
		return 0;
	}

	err = bt_gatt_notify(conn, &guitar_svc.attrs[1], &current_accel, sizeof(current_accel));
	if (err) {
		LOG_ERR("Failed to send notification (err %d)", err);
		return err;
	}

	/* Update previous values after successful send */
	previous_accel = current_accel;
	
	LOG_DBG("Sent accel: X=%d, Y=%d, Z=%d milli-g", 
		current_accel.x, current_accel.y, current_accel.z);

	return 0;
}

/* ========== MAIN FUNCTION ========== */

int main(void)
{
	int err;
	struct sensor_value accel[3];

	LOG_INF("BLE Peripheral starting (simplified for troubleshooting)...");

#if TEST_MODE_ENABLED
	LOG_INF("Running in TEST MODE - no accelerometer required");
#else
	/* Initialize accelerometer */
	if (!device_is_ready(accel_dev)) {
		LOG_ERR("Accelerometer device not ready");
		return 0;
	}
	LOG_INF("Accelerometer initialized");

	/* Configure GPIO interrupt for ADXL362 INT1 pin */
	const struct gpio_dt_spec int1_gpio = GPIO_DT_SPEC_GET_BY_IDX(DT_ALIAS(accel0), int1_gpios, 0);
	
	if (!gpio_is_ready_dt(&int1_gpio)) {
		LOG_ERR("INT1 GPIO not ready");
		return 0;
	}
	
	err = gpio_pin_configure_dt(&int1_gpio, GPIO_INPUT);
	if (err) {
		LOG_ERR("Failed to configure INT1 as input (err %d)", err);
		return 0;
	}
	
	err = gpio_pin_interrupt_configure_dt(&int1_gpio, GPIO_INT_EDGE_RISING);
	if (err) {
		LOG_ERR("Failed to configure interrupt (err %d)", err);
		return 0;
	}
	
	gpio_init_callback(&accel_cb_data, accel_gpio_handler, BIT(int1_gpio.pin));
	err = gpio_add_callback(int1_gpio.port, &accel_cb_data);
	if (err) {
		LOG_ERR("Failed to add callback (err %d)", err);
		return 0;
	}
	
	LOG_INF("ADXL362 motion interrupt enabled on GPIO0.%d (threshold: 500mg)", int1_gpio.pin);
#endif

	// COMMENTED OUT FOR TROUBLESHOOTING
	// /* Initialize motion timer */
	// k_timer_init(&motion_timer, motion_timeout_handler, NULL);

	/* Initialize LEDs and buttons */
	err = dk_leds_init();
	if (err) {
		LOG_ERR("LEDs init failed (err %d)", err);
		return 0;
	}

	err = init_button();
	if (err) {
		LOG_ERR("Button init failed (err %d)", err);
		return 0;
	}

	/* Start LED blink thread for "connected but not transmitting" indication */
	k_thread_create(&led_blink_thread, led_blink_stack,
	                K_THREAD_STACK_SIZEOF(led_blink_stack),
	                led_blink_thread_fn, NULL, NULL, NULL,
	                7, 0, K_NO_WAIT);
	k_thread_name_set(&led_blink_thread, "led_blink");

	/* Register authentication callbacks */

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

	is_advertising = true;
	LOG_INF("Advertising successfully started, device name: %s", DEVICE_NAME);

	/* Initialize spike limiter with zero values */
	spike_limiter_init(NULL);
	LOG_INF("Spike limiter initialized (limit: %d milli-g)", SPIKE_LIMIT_MILLI_G);

#if ENABLE_RUNNING_AVERAGE
	/* Initialize running average filter */
	running_average_init();
	LOG_INF("Running average filter initialized (depth: %d)", RUNNING_AVERAGE_DEPTH);
#else
	LOG_INF("Running average filter disabled");
#endif

	// COMMENTED OUT FOR TROUBLESHOOTING
	// k_timer_start(&motion_timer, K_MSEC(MOTION_TIMEOUT_MS), K_NO_WAIT);

	/* Main loop: Generate test data and send notifications */
	LOG_INF("Entering main loop (TEST_MODE enabled)...");
	while (1) {
#if TEST_MODE_ENABLED
		/* Generate synthetic incrementing test data */
		current_accel.x = test_counter;
		current_accel.y = test_counter + 100;
		current_accel.z = test_counter + 200;
		
		test_counter += 10;
		if (test_counter > 1000) {
			test_counter = 0;
		}
		
		/* Send notification if connected and enabled */
		if (is_connected) {
			bt_conn_foreach(BT_CONN_TYPE_LE, 
				       (void (*)(struct bt_conn *, void *))send_accel_notification,
				       NULL);
		}
		
		k_sleep(K_MSEC(100)); /* 10Hz update rate */
#else
		/* Read accelerometer and send notifications */
		err = sensor_sample_fetch(accel_dev);
		if (err == 0) {
			struct accel_data raw_accel, filtered_accel;
			
			sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
			
			/* Get acceleration values in m/s² */
			double x = sensor_value_to_double(&accel[0]);
			double y = sensor_value_to_double(&accel[1]);
			double z = sensor_value_to_double(&accel[2]);
			
			/* Convert to milli-g */
			convert_accel_to_milli_g(x, y, z, &raw_accel);
			
			/* Apply spike limiter */
			apply_spike_limiter(&raw_accel, &filtered_accel);
			
#if ENABLE_RUNNING_AVERAGE
			/* Apply running average filter */
			apply_running_average(&filtered_accel, &current_accel);
#else
			/* No running average - use spike-limited data directly */
			current_accel = filtered_accel;
#endif
			
			/* Send notification if connected and enabled (only if data changed) */
			if (is_connected) {
				bt_conn_foreach(BT_CONN_TYPE_LE, 
					       (void (*)(struct bt_conn *, void *))send_accel_notification,
					       NULL);
			}
		} else {
			LOG_ERR("Failed to fetch sensor sample (err %d)", err);
		}
		
		k_sleep(K_MSEC(100)); /* 10Hz sample rate */
#endif
	}

	return 0;
}

/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Integration Tests - Software-in-the-Loop Testing
 * 
 * Tests the complete data flow:
 *   1. Client generates motion data
 *   2. Client converts to milli-g and sends via BLE
 *   3. Basestation receives and converts to MIDI CC
 *   4. Verify MIDI output correctness
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "ble_hal.h"
#include "client_emulator.h"
#include "basestation_emulator.h"

/* Test utilities */
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

#define TEST_START(name) \
	do { \
		printf("\n--- Test %d: %s ---\n", ++test_count, name); \
	} while (0)

#define TEST_ASSERT(cond, msg) \
	do { \
		if (!(cond)) { \
			printf("  ❌ FAILED: %s\n", msg); \
			test_failed++; \
			return; \
		} \
	} while (0)

#define TEST_PASS() \
	do { \
		printf("  ✅ PASSED\n"); \
		test_passed++; \
	} while (0)

/* ============================================================================
 * Test Scenarios
 * ============================================================================ */

/**
 * Test 1: Connection Establishment
 * - Basestation starts scanning
 * - Client starts advertising
 * - Basestation discovers client
 * - Basestation connects to client
 * - Verify connection successful
 */
static void test_connection_establishment(void)
{
	TEST_START("Connection Establishment");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	
	/* Initialize */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	
	/* Start basestation scanning */
	TEST_ASSERT(basestation_emulator_start_scan(&base) == 0, "Start scan failed");
	
	/* Start client advertising */
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	
	/* Process BLE events (discovery should happen) */
	ble_hal_process_events();
	
	/* Basestation should discover the client - connect to it */
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	
	/* Process connection event */
	ble_hal_process_events();
	
	/* Verify connection */
	TEST_ASSERT(client_emulator_is_connected(&client), "Client not connected");
	TEST_ASSERT(basestation_emulator_get_num_guitars(&base) == 1, "Basestation has wrong guitar count");
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 2: Notification Enablement
 * - Establish connection
 * - Basestation enables notifications
 * - Verify notifications enabled
 */
static void test_notification_enable(void)
{
	TEST_START("Notification Enablement");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	
	/* Initialize and connect */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	
	ble_hal_process_events();
	
	/* Enable notifications */
	TEST_ASSERT(basestation_emulator_enable_notifications(&base, 0) == 0,
	            "Enable notifications failed");
	
	ble_hal_process_events();
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 3: Simple Data Flow
 * - Establish connection and enable notifications
 * - Client sends one acceleration packet
 * - Verify basestation receives it
 * - Verify MIDI conversion is correct
 */
static void test_simple_data_flow(void)
{
	TEST_START("Simple Data Flow");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	
	/* Setup */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_enable_notifications(&base, 0) == 0,
	            "Enable notifications failed");
	ble_hal_process_events();
	
	/* Send acceleration data: 1000 milli-g on each axis (1g) */
	struct accel_data accel = {1000, 1000, 1000};
	TEST_ASSERT(client_emulator_send_accel(&client, &accel) == 0, "Send accel failed");
	
	/* Process notification */
	ble_hal_process_events();
	
	/* Verify basestation received data */
	TEST_ASSERT(base.packets_received == 1, "Basestation didn't receive packet");
	TEST_ASSERT(base.midi_messages_sent == 3, "Wrong number of MIDI messages");
	
	/* Verify MIDI values */
	uint8_t midi_msg[3];
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 0, midi_msg), "No MIDI X data");
	
	/* Expected MIDI value for 1000 milli-g:
	 * ((1000 + 2000) * 127) / 4000 = (3000 * 127) / 4000 = 95.25 ≈ 95 */
	uint8_t expected_value = 95;
	TEST_ASSERT(midi_msg[2] == expected_value,
	            "MIDI value incorrect (expected 95, got actual)");
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 4: Multiple Packets
 * - Send multiple acceleration packets
 * - Verify all are received and converted correctly
 */
static void test_multiple_packets(void)
{
	TEST_START("Multiple Packets");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	
	/* Setup */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_enable_notifications(&base, 0) == 0,
	            "Enable notifications failed");
	ble_hal_process_events();
	
	/* Send 10 packets with different values */
	for (int i = 0; i < 10; i++) {
		struct accel_data accel = {
			(int16_t)(i * 100),
			(int16_t)(i * 100),
			(int16_t)(i * 100)
		};
		TEST_ASSERT(client_emulator_send_accel(&client, &accel) == 0, "Send accel failed");
		ble_hal_process_events();
	}
	
	/* Verify all received */
	TEST_ASSERT(base.packets_received == 10, "Not all packets received");
	TEST_ASSERT(base.midi_messages_sent == 30, "Wrong MIDI message count");
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 5: Motion Detection Threshold
 * - Send data below motion threshold
 * - Verify no notification sent
 * - Send data above threshold
 * - Verify notification sent
 */
static void test_motion_threshold(void)
{
	TEST_START("Motion Detection Threshold");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	
	/* Setup */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_enable_notifications(&base, 0) == 0,
	            "Enable notifications failed");
	ble_hal_process_events();
	
	/* Send small motion (below 0.5 m/s² threshold = ~51 milli-g) */
	struct accel_data small = {10, 10, 10};  /* magnitude ~17 milli-g */
	client_emulator_update_accel(&client, 0.1, 0.1, 0.1);  /* Small values in m/s² */
	
	/* No notification should be sent */
	TEST_ASSERT(base.packets_received == 0, "Packet sent for small motion");
	
	/* Send large motion (above threshold) */
	struct accel_data large = {100, 100, 100};  /* magnitude ~173 milli-g > 51 */
	client_emulator_update_accel(&client, 1.0, 1.0, 1.0);  /* Larger values in m/s² */
	ble_hal_process_events();
	
	/* Notification should be sent */
	TEST_ASSERT(base.packets_received == 1, "No packet sent for large motion");
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 6: MIDI Value Range
 * - Test edge cases: min, max, zero
 * - Verify MIDI values are clamped correctly
 */
static void test_midi_range(void)
{
	TEST_START("MIDI Value Range");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	uint8_t midi_msg[3];
	
	/* Setup */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_enable_notifications(&base, 0) == 0,
	            "Enable notifications failed");
	ble_hal_process_events();
	
	/* Test minimum value (-2000 milli-g should map to MIDI 0) */
	struct accel_data min_accel = {-2000, -2000, -2000};
	TEST_ASSERT(client_emulator_send_accel(&client, &min_accel) == 0, "Send failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 0, midi_msg), "No MIDI data");
	TEST_ASSERT(midi_msg[2] == 0, "Min value not mapped to MIDI 0");
	
	/* Test zero (0 milli-g should map to MIDI 64) */
	struct accel_data zero_accel = {0, 0, 0};
	TEST_ASSERT(client_emulator_send_accel(&client, &zero_accel) == 0, "Send failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 0, midi_msg), "No MIDI data");
	TEST_ASSERT(midi_msg[2] == 63 || midi_msg[2] == 64, "Zero not mapped to MIDI 63-64");
	
	/* Test maximum value (+2000 milli-g should map to MIDI 127) */
	struct accel_data max_accel = {2000, 2000, 2000};
	TEST_ASSERT(client_emulator_send_accel(&client, &max_accel) == 0, "Send failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 0, midi_msg), "No MIDI data");
	TEST_ASSERT(midi_msg[2] == 127, "Max value not mapped to MIDI 127");
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 7: MIDI Message Format
 * - Verify MIDI CC message structure
 * - Check status byte, controller number, and value
 */
static void test_midi_format(void)
{
	TEST_START("MIDI Message Format");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	uint8_t midi_msg[3];
	
	/* Setup */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	ble_hal_process_events();
	TEST_ASSERT(basestation_emulator_enable_notifications(&base, 0) == 0,
	            "Enable notifications failed");
	ble_hal_process_events();
	
	/* Send data */
	struct accel_data accel = {1000, 500, -500};
	TEST_ASSERT(client_emulator_send_accel(&client, &accel) == 0, "Send failed");
	ble_hal_process_events();
	
	/* Check X-axis MIDI message */
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 0, midi_msg), "No X MIDI data");
	TEST_ASSERT(midi_msg[0] == 0xB0, "Wrong status byte (should be 0xB0 for CC on channel 0)");
	TEST_ASSERT(midi_msg[1] == 16, "Wrong CC number for X-axis (should be 16)");
	TEST_ASSERT(midi_msg[2] <= 127, "MIDI value out of range");
	
	/* Check Y-axis MIDI message */
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 1, midi_msg), "No Y MIDI data");
	TEST_ASSERT(midi_msg[0] == 0xB0, "Wrong status byte");
	TEST_ASSERT(midi_msg[1] == 17, "Wrong CC number for Y-axis (should be 17)");
	
	/* Check Z-axis MIDI message */
	TEST_ASSERT(basestation_emulator_get_last_midi(&base, 2, midi_msg), "No Z MIDI data");
	TEST_ASSERT(midi_msg[0] == 0xB0, "Wrong status byte");
	TEST_ASSERT(midi_msg[1] == 18, "Wrong CC number for Z-axis (should be 18)");
	
	/* Cleanup */
	client_emulator_cleanup(&client);
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/**
 * Test 8: Disconnection Handling
 * - Establish connection
 * - Disconnect
 * - Verify cleanup
 */
static void test_disconnection(void)
{
	TEST_START("Disconnection Handling");
	
	basestation_emulator_t base;
	client_emulator_t client;
	uint8_t client_addr[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
	
	/* Setup */
	TEST_ASSERT(basestation_emulator_init(&base) == 0, "Basestation init failed");
	TEST_ASSERT(client_emulator_init(&client, client_addr) == 0, "Client init failed");
	TEST_ASSERT(client_emulator_start_advertising(&client) == 0, "Start advertising failed");
	TEST_ASSERT(basestation_emulator_connect(&base, client_addr) == 0, "Connect failed");
	ble_hal_process_events();
	
	/* Verify connection */
	TEST_ASSERT(client_emulator_is_connected(&client), "Not connected");
	TEST_ASSERT(basestation_emulator_get_num_guitars(&base) == 1, "Wrong guitar count");
	
	/* Disconnect */
	client_emulator_cleanup(&client);
	ble_hal_process_events();
	
	/* Verify disconnection */
	TEST_ASSERT(basestation_emulator_get_num_guitars(&base) == 0, "Guitar not removed");
	
	/* Cleanup */
	basestation_emulator_cleanup(&base);
	
	TEST_PASS();
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(void)
{
	printf("\n");
	printf("========================================\n");
	printf("  Integration Tests - SIL Framework\n");
	printf("========================================\n");
	printf("\n");
	
	/* Initialize BLE HAL */
	ble_hal_init();
	
	/* Run test suite */
	test_connection_establishment();
	test_notification_enable();
	test_simple_data_flow();
	test_multiple_packets();
	test_motion_threshold();
	test_midi_range();
	test_midi_format();
	test_disconnection();
	
	/* Print summary */
	printf("\n");
	printf("========================================\n");
	printf("  Test Summary\n");
	printf("========================================\n");
	printf("  Total:  %d\n", test_count);
	printf("  Passed: %d ✅\n", test_passed);
	printf("  Failed: %d ❌\n", test_failed);
	printf("========================================\n");
	printf("\n");
	
	return (test_failed == 0) ? 0 : 1;
}

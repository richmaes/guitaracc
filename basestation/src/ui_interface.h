/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include <zephyr/kernel.h>
#include <stdbool.h>

/**
 * @brief Initialize the UI interface (Zephyr Shell)
 * 
 * @return 0 on success, negative error code on failure
 */
int ui_interface_init(void);

/**
 * @brief Update connected devices count
 * 
 * @param count Number of connected devices
 */
void ui_set_connected_devices(int count);

/**
 * @brief Update MIDI output active state
 * 
 * @param active True if MIDI output is active
 */
void ui_set_midi_output_active(bool active);

/**
 * @brief MIDI receive statistics structure
 */
struct midi_rx_stats {
	uint32_t total_bytes;
	uint32_t clock_messages;  /* 0xF8 MIDI Timing Clock */
	uint32_t start_messages;  /* 0xFA MIDI Start */
	uint32_t continue_messages; /* 0xFB MIDI Continue */
	uint32_t stop_messages;   /* 0xFC MIDI Stop */
	uint32_t other_messages;
	uint32_t last_clock_time; /* Timestamp of last clock message */
	uint32_t clock_interval_us; /* Interval between clocks in microseconds */
};

/**
 * @brief Get MIDI receive statistics
 * 
 * @param stats Output buffer for statistics
 */
void ui_get_midi_rx_stats(struct midi_rx_stats *stats);

/**
 * @brief Reset MIDI receive statistics
 */
void ui_reset_midi_rx_stats(void);

/**
 * @brief Send MIDI real-time message (high priority)
 * 
 * Real-time messages (0xF8-0xFF) are sent via priority queue
 * and will preempt regular MIDI messages.
 * 
 * @param rt_byte Real-time message byte (0xF8-0xFF)
 * @return 0 on success, negative error code on failure
 */
int send_midi_realtime(uint8_t rt_byte);

/**
 * @brief Configuration reload callback
 * 
 * Set this callback to be notified when configuration changes via shell
 */
extern void (*ui_config_reload_callback)(void);

#endif /* UI_INTERFACE_H */
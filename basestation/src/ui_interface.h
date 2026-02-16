/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UI_INTERFACE_H
#define UI_INTERFACE_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>

/**
 * @brief Initialize the UI interface
 * 
 * @param uart_dev UART device to use for UI interface
 * @return 0 on success, negative error code on failure
 */
int ui_interface_init(const struct device *uart_dev);

/**
 * @brief Process incoming character from UI interface
 * Called from UART ISR
 * 
 * @param c Character received
 */
void ui_interface_process_char(char c);

/**
 * @brief Print a message to the UI interface
 * 
 * @param fmt Printf-style format string
 * @param ... Format arguments
 */
void ui_print(const char *fmt, ...);

/**
 * @brief Update system status information
 * Called by main application when status changes
 * 
 * @param connected_count Number of connected devices
 * @param midi_active True if MIDI output is active
 */
void ui_interface_update_status(int connected_count, bool midi_active);

#endif /* UI_INTERFACE_H */

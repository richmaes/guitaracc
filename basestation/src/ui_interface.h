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
 * @brief Configuration reload callback
 * 
 * Set this callback to be notified when configuration changes via shell
 */
extern void (*ui_config_reload_callback)(void);

#endif /* UI_INTERFACE_H */
/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_interface.h"
#include "config_storage.h"
#include "topology_config.h"
#include "function_units.h"
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/data/json.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ui_shell, LOG_LEVEL_DBG);

/* System status */
static int connected_devices = 0;
static bool midi_output_active = false;

/* Configuration reload callback */
void (*ui_config_reload_callback)(void) = NULL;

/*
 * Shell command handlers
 */

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	/* Show config storage info */
	enum config_area active_area;
	uint32_t sequence;
	
	if (config_storage_get_info(&active_area, &sequence) == 0) {
		const char *area_name = (active_area == CONFIG_AREA_A) ? "A" : "B";
		shell_print(sh, "Config area: %s (seq=%u)", area_name, sequence);
	}
	
	shell_print(sh, "\n=== GuitarAcc Basestation Status ===");
	shell_print(sh, "Connected devices: %d", connected_devices);
	shell_print(sh, "MIDI output: %s", midi_output_active ? "Active" : "Inactive");
	
	return 0;
}

static int cmd_config_show(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	static struct config_data cfg;
	
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	shell_print(sh, "\n=== Configuration ===");
	
	shell_print(sh, "\n--- GLOBAL SETTINGS ---");
	shell_print(sh, "Active patch: %d", cfg.global.default_patch);
	shell_print(sh, "MIDI:");
	shell_print(sh, "  Channel: %d", cfg.global.midi_channel + 1);
	shell_print(sh, "BLE:");
	shell_print(sh, "  Max guitars: %d", cfg.global.max_guitars);
	shell_print(sh, "  Scan interval: %d ms", cfg.global.scan_interval_ms);
	shell_print(sh, "LED:");
	shell_print(sh, "  Brightness: %d", cfg.global.led_brightness);
	shell_print(sh, "Filters:");
	shell_print(sh, "  Running average: %s", cfg.global.running_average_enable ? "Enabled" : "Disabled");
	shell_print(sh, "  Average depth: %d samples", cfg.global.running_average_depth);
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	shell_print(sh, "\n--- PATCH SETTINGS (Patch %d) ---", patch_idx);
	shell_print(sh, "Name: %s", cfg.patches[patch_idx].patch_name);
	shell_print(sh, "MIDI:");
	shell_print(sh, "LED:");
	shell_print(sh, "  Mode: %d", cfg.patches[patch_idx].led_mode);
	shell_print(sh, "Accelerometer:");
	shell_print(sh, "  Deadzone: %d", cfg.patches[patch_idx].midi_deadzone);
	
	return 0;
}

static int cmd_config_save(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	static struct config_data cfg;
	
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Configuration saved to flash");
	return 0;
}

static int cmd_config_restore(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	if (config_storage_restore_defaults() != 0) {
		shell_error(sh, "Error restoring defaults");
		return -1;
	}
	
	shell_print(sh, "Factory defaults restored");
	return 0;
}

static int cmd_config_midi_ch(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config midi_ch <1-16>");
		return -1;
	}
	
	int ch = atoi(argv[1]);
	if (ch < 1 || ch > 16) {
		shell_error(sh, "Invalid channel (1-16)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.midi_channel = ch - 1;  /* 0-indexed internally */
	
	int ret = config_storage_save(&cfg);
	if (ret != 0) {
		shell_error(sh, "Error saving configuration (code: %d)", ret);
		return -1;
	}
	
	shell_print(sh, "MIDI channel set to %d (global setting)", ch);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

/* Legacy function - removed (cc_mapping field no longer exists)
static int cmd_config_cc(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: config cc <x|y|z> <0-127>");
		return -1;
	}
	
	int axis = -1;
	if (strcmp(argv[1], "x") == 0 || strcmp(argv[1], "X") == 0) axis = 0;
	else if (strcmp(argv[1], "y") == 0 || strcmp(argv[1], "Y") == 0) axis = 1;
	else if (strcmp(argv[1], "z") == 0 || strcmp(argv[1], "Z") == 0) axis = 2;
	else {
		shell_error(sh, "Invalid axis. Use x, y, or z");
		return -1;
	}
	
	int cc_num = atoi(argv[2]);
	if (cc_num < 0 || cc_num > 127) {
		shell_error(sh, "Invalid CC number (0-127)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	cfg.patches[patch_idx].cc_mapping[axis] = cc_num;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	const char *axis_names[] = {"X", "Y", "Z"};
	shell_print(sh, "%s-axis CC set to %d (patch %d setting)", axis_names[axis], cc_num, patch_idx);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}
*/

/* Legacy functions - removed (accel_min, accel_max, accel_invert fields no longer exist)
static int cmd_config_accel_min(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: config accel_min <0-5> <0-127>");
		return -1;
	}
	
	int axis = atoi(argv[1]);
	if (axis < 0 || axis > 5) {
		shell_error(sh, "Invalid axis (0-5)");
		return -1;
	}
	
	int value = atoi(argv[2]);
	if (value < 0 || value > 127) {
		shell_error(sh, "Invalid value (0-127)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	cfg.patches[patch_idx].accel_min[axis] = (uint8_t)value;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Axis %d min CC set to %d (patch %d)", axis, value, patch_idx);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_accel_max(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: config accel_max <0-5> <0-127>");
		return -1;
	}
	
	int axis = atoi(argv[1]);
	if (axis < 0 || axis > 5) {
		shell_error(sh, "Invalid axis (0-5)");
		return -1;
	}
	
	int value = atoi(argv[2]);
	if (value < 0 || value > 127) {
		shell_error(sh, "Invalid value (0-127)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	cfg.patches[patch_idx].accel_max[axis] = (uint8_t)value;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Axis %d max CC set to %d (patch %d)", axis, value, patch_idx);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_accel_invert(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: config accel_invert <0-5> <0|1>");
		return -1;
	}
	
	int axis = atoi(argv[1]);
	if (axis < 0 || axis > 5) {
		shell_error(sh, "Invalid axis (0-5)");
		return -1;
	}
	
	int invert = atoi(argv[2]);
	if (invert != 0 && invert != 1) {
		shell_error(sh, "Invalid value (0=normal, 1=invert)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	if (invert) {
		cfg.patches[patch_idx].accel_invert |= (1 << axis);
	} else {
		cfg.patches[patch_idx].accel_invert &= ~(1 << axis);
	}
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Axis %d invert set to %s (patch %d)", axis, invert ? "ON" : "OFF", patch_idx);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}
*/

static int cmd_config_accel_deadzone(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config accel_deadzone <0-127>");
		return -1;
	}
	
	int deadzone = atoi(argv[1]);
	if (deadzone < 0 || deadzone > 127) {
		shell_error(sh, "Invalid deadzone (0-127)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	cfg.patches[patch_idx].midi_deadzone = (int16_t)deadzone;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "CC change threshold set to %d (patch %d)", deadzone, patch_idx);
	shell_print(sh, "MIDI CC will only be sent when value changes by >= %d", deadzone);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

/* Legacy functions - removed (accel_scale and accel_offset fields no longer exist)
static int cmd_config_accel_scale(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: config accel_scale <axis:0-2> <scale_mg:100-4000>");
		shell_error(sh, "  axis: 0=X, 1=Y, 2=Z");
		shell_error(sh, "  scale_mg: G-force in milli-g that maps to full MIDI range");
		shell_error(sh, "  Examples:");
		shell_error(sh, "    500  = ±0.5g maps to MIDI 0-127 (high sensitivity)");
		shell_error(sh, "    1000 = ±1.0g maps to MIDI 0-127 (medium sensitivity)");
		shell_error(sh, "    2000 = ±2.0g maps to MIDI 0-127 (low sensitivity, default)");
		return -1;
	}
	
	int axis = atoi(argv[1]);
	if (axis < 0 || axis > 2) {
		shell_error(sh, "Invalid axis (0=X, 1=Y, 2=Z)");
		return -1;
	}
	
	int scale_mg = atoi(argv[2]);
	if (scale_mg < 100 || scale_mg > 4000) {
		shell_error(sh, "Invalid scale (100-4000 mg = 0.1g-4.0g)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.accel_scale[axis] = (int16_t)scale_mg;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	const char *axis_names[] = {"X", "Y", "Z"};
	shell_print(sh, "%s-axis scale set to ±%dmg (±%.1fg)", 
		axis_names[axis], scale_mg, scale_mg / 1000.0);
	shell_print(sh, "Accelerometer range ±%dmg now maps to MIDI 0-127", scale_mg);
	shell_print(sh, "Values beyond this range will be clamped");
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_accel_offset(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 3) {
		shell_error(sh, "Usage: config accel_offset <axis:0-2> <offset_mg:-2000-2000>");
		shell_error(sh, "  axis: 0=X, 1=Y, 2=Z");
		shell_error(sh, "  offset_mg: Accelerometer value (in milli-g) that maps to MIDI 64");
		shell_error(sh, "  Examples:");
		shell_error(sh, "    0    = 0g (neutral) maps to MIDI 64 (default)");
		shell_error(sh, "    200  = +0.2g maps to MIDI 64");
		shell_error(sh, "    -200 = -0.2g maps to MIDI 64");
		return -1;
	}
	
	int axis = atoi(argv[1]);
	if (axis < 0 || axis > 2) {
		shell_error(sh, "Invalid axis (0=X, 1=Y, 2=Z)");
		return -1;
	}
	
	int offset_mg = atoi(argv[2]);
	if (offset_mg < -2000 || offset_mg > 2000) {
		shell_error(sh, "Invalid offset (-2000 to 2000 mg = -2.0g to 2.0g)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.accel_offset[axis] = (int16_t)offset_mg;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	const char *axis_names[] = {"X", "Y", "Z"};
	shell_print(sh, "%s-axis offset set to %dmg (%.2fg)", 
		axis_names[axis], offset_mg, offset_mg / 1000.0);
	shell_print(sh, "This accelerometer value now maps to MIDI 64 (center)");
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}
*/

/* Legacy function - removed (velocity_curve field no longer exists)
static int cmd_config_velocity_curve(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config velocity_curve <0-127>");
		return -1;
	}
	
	int curve = atoi(argv[1]);
	if (curve < 0 || curve > 127) {
		shell_error(sh, "Invalid velocity curve (0-127)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	cfg.patches[patch_idx].velocity_curve = (uint8_t)curve;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Velocity curve set to %d (patch %d setting)", curve, patch_idx);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}
*/

static int cmd_config_scan_interval(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config scan_interval <10-1000>");
		return -1;
	}
	
	int interval = atoi(argv[1]);
	if (interval < 10 || interval > 1000) {
		shell_error(sh, "Invalid interval (10-1000 ms)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.scan_interval_ms = (uint8_t)interval;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "BLE scan interval set to %d ms (global setting)", interval);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_avg_enable(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config avg_enable <0|1>");
		return -1;
	}
	
	int enable = atoi(argv[1]);
	if (enable != 0 && enable != 1) {
		shell_error(sh, "Invalid value (0=disable, 1=enable)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.running_average_enable = (uint8_t)enable;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Running average %s (global setting)", enable ? "enabled" : "disabled");
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_avg_depth(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config avg_depth <3-10>");
		return -1;
	}
	
	int depth = atoi(argv[1]);
	if (depth < 3 || depth > 10) {
		shell_error(sh, "Invalid depth (3-10 samples)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.running_average_depth = (uint8_t)depth;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Running average depth set to %d samples (global setting)", depth);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_midi_rx_stats(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	struct midi_rx_stats stats;
	ui_get_midi_rx_stats(&stats);
	
	shell_print(sh, "\n=== MIDI RX Statistics ===");
	shell_print(sh, "Total bytes received: %u", stats.total_bytes);
	shell_print(sh, "Clock messages (0xF8): %u", stats.clock_messages);
	
	if (stats.clock_messages > 0 && stats.clock_interval_us > 0) {
		/* Calculate BPM from clock interval */
		/* MIDI clock runs at 24 pulses per quarter note */
		/* BPM = (60,000,000 / interval_us) / 24 */
		uint32_t bpm = (60000000 / stats.clock_interval_us) / 24;
		shell_print(sh, "Clock interval: %u us (~%u BPM)", 
			    stats.clock_interval_us, bpm);
	}
	
	shell_print(sh, "Start messages (0xFA): %u", stats.start_messages);
	shell_print(sh, "Continue messages (0xFB): %u", stats.continue_messages);
	shell_print(sh, "Stop messages (0xFC): %u", stats.stop_messages);
	shell_print(sh, "Other messages: %u", stats.other_messages);
	
	return 0;
}

static int cmd_midi_rx_reset(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	ui_reset_midi_rx_stats();
	shell_print(sh, "MIDI RX statistics reset");
	
	return 0;
}

static int cmd_midi_program(const struct shell *sh, size_t argc, char **argv)
{
	if (argc == 1) {
		/* No argument - show current program */
		uint8_t program = ui_get_current_program();
		shell_print(sh, "Current MIDI Program: %d", program);
	} else {
		/* Set program */
		int program = atoi(argv[1]);
		
		if (program < 0 || program > 127) {
			shell_error(sh, "Program must be 0-127");
			return -1;
		}
		
		ui_set_current_program((uint8_t)program);
		shell_print(sh, "MIDI Program set to %d", program);
	}
	
	return 0;
}

static int cmd_midi_send_rt(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: midi send_rt <0xF8-0xFF>");
		shell_print(sh, "Common real-time messages:");
		shell_print(sh, "  0xF8 - Timing Clock");
		shell_print(sh, "  0xFA - Start");
		shell_print(sh, "  0xFB - Continue");
		shell_print(sh, "  0xFC - Stop");
		shell_print(sh, "  0xFE - Active Sensing");
		shell_print(sh, "  0xFF - Reset");
		return -1;
	}
	
	/* Parse hex value */
	unsigned long rt_byte = strtoul(argv[1], NULL, 16);
	
	if (rt_byte < 0xF8 || rt_byte > 0xFF) {
		shell_error(sh, "Invalid real-time byte (must be 0xF8-0xFF)");
		return -1;
	}
	
	int err = send_midi_realtime((uint8_t)rt_byte);
	if (err != 0) {
		shell_error(sh, "Failed to send real-time message (err %d)", err);
		return err;
	}
	
	shell_print(sh, "Sent real-time message: 0x%02X", (uint8_t)rt_byte);
	return 0;
}

static int cmd_config_patch(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config patch <0-15>");
		return -1;
	}
	
	int patch_num = atoi(argv[1]);
	if (patch_num < 0 || patch_num > 15) {
		shell_error(sh, "Invalid patch number (0-15)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t patch_idx = (uint8_t)patch_num;
	bool is_active = (patch_idx == cfg.global.default_patch);
	
	shell_print(sh, "\n--- PATCH %d%s ---", patch_idx, is_active ? " (ACTIVE)" : "");
	shell_print(sh, "Name: %s", cfg.patches[patch_idx].patch_name);
	shell_print(sh, "LED:");
	shell_print(sh, "  Mode: %d", cfg.patches[patch_idx].led_mode);
	shell_print(sh, "Accelerometer:");
	shell_print(sh, "  Deadzone: %d", cfg.patches[patch_idx].midi_deadzone);
	
	return 0;
}

static int cmd_config_select_patch(const struct shell *sh, size_t argc, char **argv)
{
	if (argc != 2) {
		shell_error(sh, "Usage: config select <0-15>");
		return -1;
	}
	
	int patch_num = atoi(argv[1]);
	if (patch_num < 0 || patch_num > 15) {
		shell_error(sh, "Invalid patch number (0-15)");
		return -1;
	}
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.global.default_patch = (uint8_t)patch_num;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Active patch changed to %d (%s)", 
		    patch_num, cfg.patches[patch_num].patch_name);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_list_patches(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	uint8_t active = cfg.global.default_patch;
	
	shell_print(sh, "\n=== Patches (0-15) ===");
	shell_print(sh, "Active patch: %d\n", active);
	
	/* Show first 10 patches as a sample */
	for (int i = 0; i < 10; i++) {
		shell_print(sh, "%c %3d: %s",
			    (i == active) ? '*' : ' ',
			    i,
			    cfg.patches[i].patch_name);
	}
	
	shell_print(sh, "  ...  (use 'config patch <num>' to view specific patch)");
	shell_print(sh, "\nUse 'config select <num>' to change active patch");
	
	return 0;
}

static int cmd_config_erase_all(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_warn(sh, "*** WARNING: ERASE ALL CONFIGURATION STORAGE ***");
	shell_warn(sh, "This will erase AREA_A and AREA_B");
	shell_warn(sh, "Device will use hardcoded defaults on next boot");
	shell_warn(sh, "This command is for TESTING ONLY!");
	
	if (config_storage_erase_all() != 0) {
		shell_error(sh, "Error erasing configuration storage");
		return -1;
	}
	
	shell_print(sh, "All configuration erased successfully");
	shell_warn(sh, "*** REBOOT REQUIRED ***");
	shell_print(sh, "Use 'kernel reboot cold' or power cycle");
	
	return 0;
}

/* Helper function to print JSON boolean */
static const char *json_bool(bool value)
{
	return value ? "true" : "false";
}

/* Helper function to print patch as JSON */
static void print_patch_json(const struct shell *sh, const struct patch_config *patch, int patch_num, bool last)
{
	shell_print(sh, "      {");
	shell_print(sh, "        \"patch_num\": %d,", patch_num);
	shell_print(sh, "        \"patch_name\": \"%s\",", patch->patch_name);
	shell_print(sh, "        \"led_mode\": %d,", patch->led_mode);
	shell_print(sh, "        \"midi_deadzone\": %d", patch->midi_deadzone);
	shell_print(sh, "      }%s", last ? "" : ",");
}

static int cmd_config_export(const struct shell *sh, size_t argc, char **argv)
{
	static struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	/* Determine export type: full, global, or single patch */
	bool export_global = false;
	bool export_patches = false;
	int single_patch = -1;
	
	if (argc == 1) {
		/* No arguments - export everything */
		export_global = true;
		export_patches = true;
	} else if (argc == 2 && strcmp(argv[1], "global") == 0) {
		/* Export global only */
		export_global = true;
	} else if (argc == 3 && strcmp(argv[1], "patch") == 0) {
		/* Export single patch */
		single_patch = atoi(argv[2]);
		if (single_patch < 0 || single_patch > 15) {
			shell_error(sh, "Invalid patch number (0-15)");
			return -1;
		}
		export_patches = true;
	} else {
		shell_error(sh, "Usage: config export [global | patch <0-15>]");
		return -1;
	}
	
	/* Print JSON header */
	shell_print(sh, "{");
	shell_print(sh, "  \"version\": 1,");
	shell_print(sh, "  \"config\": {");
	
	/* Export global section */
	if (export_global) {
		shell_print(sh, "    \"global\": {");
		shell_print(sh, "      \"default_patch\": %d,", cfg.global.default_patch);
		shell_print(sh, "      \"midi_channel\": %d,", cfg.global.midi_channel);
		shell_print(sh, "      \"max_guitars\": %d,", cfg.global.max_guitars);
		shell_print(sh, "      \"ble_scan_interval_ms\": %d,", cfg.global.scan_interval_ms);
		shell_print(sh, "      \"led_brightness\": %d,", cfg.global.led_brightness);
		shell_print(sh, "      \"running_average_enable\": %s,", json_bool(cfg.global.running_average_enable));
		shell_print(sh, "      \"running_average_depth\": %d", cfg.global.running_average_depth);
		shell_print(sh, "    }%s", export_patches ? "," : "");
	}
	
	/* Export patches section */
	if (export_patches) {
		shell_print(sh, "    \"patches\": [");
		
		if (single_patch >= 0) {
			/* Export single patch */
			print_patch_json(sh, &cfg.patches[single_patch], single_patch, true);
		} else {
			/* Export all patches */
			for (int i = 0; i < 16; i++) {
				print_patch_json(sh, &cfg.patches[i], i, (i == 15));
			}
		}
		
		shell_print(sh, "    ]");
	}
	
	/* Print JSON footer */
	shell_print(sh, "  }");
	shell_print(sh, "}");
	
	return 0;
}

/*
 * Topology commands
 */

static int cmd_topo_show(const struct shell *sh, size_t argc, char **argv)
{
	struct config_data cfg;
	int err = config_storage_load(&cfg);
	if (err) {
		shell_error(sh, "Failed to load configuration");
		return err;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	shell_print(sh, "Virtual Ports Topology - Patch %d [ALWAYS ACTIVE]", patch_idx);
	shell_print(sh, "Default Mixer: %d (0=PASSTHROUGH, 1=SUM, 2=AVERAGE, 3=MAX, 4=MIN)",
		cfg.patches[patch_idx].default_mixer_type);
	shell_print(sh, "");
	
	for (int i = 0; i < MAX_TOPOLOGY_INSTANCES; i++) {
		struct topology_instance *topo = &cfg.patches[patch_idx].topologies[i];
		if (!topo->enabled) continue;
		
		shell_print(sh, "Instance %d: %s", i, topology_get_name(topo->topology_type));
		shell_print(sh, "  Accel inputs: [%d, %d]", topo->accel_inputs[0], topo->accel_inputs[1]);
		shell_print(sh, "  Function units: [%d, %d]", topo->func_units[0], topo->func_units[1]);
		shell_print(sh, "  MIDI CC outputs: [%d, %d]", topo->midi_outputs[0], topo->midi_outputs[1]);
	}
	
	return 0;
}

/* Legacy command - topology is now always active
static int cmd_topo_enable(const struct shell *sh, size_t argc, char **argv)
{
	struct config_data cfg;
	int err = config_storage_load(&cfg);
	if (err) {
		shell_error(sh, "Failed to load configuration");
		return err;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	int enable = atoi(argv[1]);
	cfg.patches[patch_idx].topology_enabled = (enable != 0) ? 1 : 0;
	
	err = config_storage_save(&cfg);
	if (err) {
		shell_error(sh, "Failed to save configuration");
		return err;
	}
	
	shell_print(sh, "Virtual ports %s for patch %d", 
		cfg.patches[patch_idx].topology_enabled ? "ENABLED" : "DISABLED",
		patch_idx);
	shell_print(sh, "Run 'config reload' to apply changes");
	
	return 0;
}
*/

static int cmd_topo_config(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 4) {
		shell_error(sh, "Usage: topo config <instance> <type> <accel> [func] [midi_cc]");
		shell_print(sh, "  instance: 0-5");
		shell_print(sh, "  type: 1=T1 (1 accel), 2=T2 (2 accel), 3=T3 (1 accel), 4=T4 (2 accel)");
		shell_print(sh, "  accel: axis index 0-5 (X,Y,Z,Roll,Pitch,Yaw)");
		shell_print(sh, "         For T2/T4: use comma-separated like '0,1' for X+Y axes");
		shell_print(sh, "  func: function unit index 0-7 (optional)");
		shell_print(sh, "  midi_cc: MIDI CC number 0-127 (optional)");
		shell_print(sh, "Examples:");
		shell_print(sh, "  topo config 0 1 0        # T1: X-axis");
		shell_print(sh, "  topo config 1 2 0,1      # T2: X+Y axes merged");
		return -EINVAL;
	}
	
	struct config_data cfg;
	int err = config_storage_load(&cfg);
	if (err) {
		shell_error(sh, "Failed to load configuration");
		return err;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	int instance = atoi(argv[1]);
	int type = atoi(argv[2]);
	
	if (instance < 0 || instance >= MAX_TOPOLOGY_INSTANCES) {
		shell_error(sh, "Invalid instance: %d (must be 0-%d)", instance, MAX_TOPOLOGY_INSTANCES-1);
		return -EINVAL;
	}
	
	if (type < TOPO_T1 || type > TOPO_T4) {
		shell_error(sh, "Invalid type: %d (must be 1-4)", type);
		return -EINVAL;
	}
	
	struct topology_instance *topo = &cfg.patches[patch_idx].topologies[instance];
	topo->topology_type = type;
	topo->enabled = 1;
	
	/* Parse accelerometer inputs (may be comma-separated for T2/T4) */
	uint8_t num_accel_inputs = topology_get_accel_input_count(type);
	int accel0 = 0, accel1 = 0;
	
	if (num_accel_inputs == 2) {
		/* T2 or T4 - need 2 inputs */
		if (sscanf(argv[3], "%d,%d", &accel0, &accel1) != 2) {
			shell_error(sh, "Type %s requires 2 accelerometer inputs (use comma-separated like '0,1')", 
				topology_get_name(type));
			return -EINVAL;
		}
		topo->accel_inputs[0] = accel0;
		topo->accel_inputs[1] = accel1;
	} else {
		/* T1 or T3 - single input */
		if (strchr(argv[3], ',')) {
			shell_error(sh, "Type %s only uses 1 accelerometer input (no comma needed)", 
				topology_get_name(type));
			return -EINVAL;
		}
		topo->accel_inputs[0] = atoi(argv[3]);
	}
	
	/* Validate accelerometer input indices */
	for (int i = 0; i < num_accel_inputs; i++) {
		if (topo->accel_inputs[i] >= MAX_ACCEL_SOURCES) {
			shell_error(sh, "Invalid accel input: %d (must be 0-%d)", 
				topo->accel_inputs[i], MAX_ACCEL_SOURCES-1);
			return -EINVAL;
		}
	}
	
	/* Set function units */
	uint8_t num_func_units = topology_get_function_count(type);
	if (argc >= 5) {
		/* Parse function units (may be comma-separated for T4) */
		int func0 = 0, func1 = 0;
		
		if (num_func_units == 2) {
			if (sscanf(argv[4], "%d,%d", &func0, &func1) == 2) {
				topo->func_units[0] = func0;
				topo->func_units[1] = func1;
			} else if (sscanf(argv[4], "%d", &func0) == 1) {
				/* If only one provided, use same for both */
				topo->func_units[0] = func0;
				topo->func_units[1] = func0;
			}
		} else {
			topo->func_units[0] = atoi(argv[4]);
		}
	} else {
		/* Default function units */
		topo->func_units[0] = instance;
		if (num_func_units == 2) {
			topo->func_units[1] = (instance + 1) % MAX_FUNCTION_UNITS;
		}
	}
	
	/* Set MIDI outputs */
	uint8_t num_midi_outputs = topology_get_midi_output_count(type);
	if (argc >= 6) {
		/* Parse MIDI outputs (may be comma-separated for T3/T4) */
		int midi0 = 0, midi1 = 0;
		
		if (num_midi_outputs == 2) {
			if (sscanf(argv[5], "%d,%d", &midi0, &midi1) == 2) {
				topo->midi_outputs[0] = midi0;
				topo->midi_outputs[1] = midi1;
			} else if (sscanf(argv[5], "%d", &midi0) == 1) {
				/* If only one provided, use consecutive CCs */
				topo->midi_outputs[0] = midi0;
				topo->midi_outputs[1] = midi0 + 1;
			}
		} else {
			topo->midi_outputs[0] = atoi(argv[5]);
		}
	} else {
		/* Default MIDI CCs */
		topo->midi_outputs[0] = 16 + instance;
		if (num_midi_outputs == 2) {
			topo->midi_outputs[1] = 16 + instance + 1;
		}
	}
	
	err = config_storage_save(&cfg);
	if (err) {
		shell_error(sh, "Failed to save configuration");
		return err;
	}
	
	shell_print(sh, "Topology instance %d configured as %s", instance, topology_get_name(type));
	if (num_accel_inputs == 2) {
		shell_print(sh, "  Accel inputs: [%d, %d]", topo->accel_inputs[0], topo->accel_inputs[1]);
	} else {
		shell_print(sh, "  Accel input: %d", topo->accel_inputs[0]);
	}
	if (num_func_units == 2) {
		shell_print(sh, "  Function units: [%d, %d]", topo->func_units[0], topo->func_units[1]);
	} else {
		shell_print(sh, "  Function unit: %d", topo->func_units[0]);
	}
	if (num_midi_outputs == 2) {
		shell_print(sh, "  MIDI outputs: [%d, %d]", topo->midi_outputs[0], topo->midi_outputs[1]);
	} else {
		shell_print(sh, "  MIDI output: %d", topo->midi_outputs[0]);
	}
	shell_print(sh, "Run 'config reload' to apply changes");
	
	return 0;
}

/*
 * Function commands
 */

static int cmd_func_show(const struct shell *sh, size_t argc, char **argv)
{
	struct config_data cfg;
	int err = config_storage_load(&cfg);
	if (err) {
		shell_error(sh, "Failed to load configuration");
		return err;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	int func_idx = 0;
	if (argc >= 2) {
		func_idx = atoi(argv[1]);
	}
	
	if (func_idx < 0 || func_idx >= MAX_FUNCTION_UNITS) {
		shell_error(sh, "Invalid function index: %d (must be 0-%d)", func_idx, MAX_FUNCTION_UNITS-1);
		return -EINVAL;
	}
	
	struct function_unit *func = &cfg.patches[patch_idx].functions[func_idx];
	
	shell_print(sh, "Function Unit %d:", func_idx);
	shell_print(sh, "  Type: %d %s", func->function_type, 
		func->function_type == FUNC_LINEAR ? "(LINEAR)" :
		func->function_type == FUNC_PASSTHROUGH ? "(PASSTHROUGH)" :
		func->function_type == FUNC_DEADZONE ? "(DEADZONE)" :
		func->function_type == FUNC_SCALE ? "(SCALE)" : "(OTHER)");
	shell_print(sh, "  Enabled: %d", func->enabled);
	shell_print(sh, "  Parameters: [%d, %d, %d, %d, %d, %d]",
		func->params[0], func->params[1], func->params[2],
		func->params[3], func->params[4], func->params[5]);
	
	if (func->function_type == FUNC_LINEAR) {
		shell_print(sh, "  Linear mapping: [%d,%d] → [%d,%d]",
			func->params[0], func->params[1], func->params[2], func->params[3]);
	}
	
	return 0;
}

static int cmd_func_linear(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 6) {
		shell_error(sh, "Usage: func linear <unit> <in_min> <in_max> <out_min> <out_max>");
		return -EINVAL;
	}
	
	struct config_data cfg;
	int err = config_storage_load(&cfg);
	if (err) {
		shell_error(sh, "Failed to load configuration");
		return err;
	}
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= NUM_PATCHES) patch_idx = 0;
	
	int func_idx = atoi(argv[1]);
	int16_t in_min = atoi(argv[2]);
	int16_t in_max = atoi(argv[3]);
	int16_t out_min = atoi(argv[4]);
	int16_t out_max = atoi(argv[5]);
	
	if (func_idx < 0 || func_idx >= MAX_FUNCTION_UNITS) {
		shell_error(sh, "Invalid function index: %d", func_idx);
		return -EINVAL;
	}
	
	struct function_unit *func = &cfg.patches[patch_idx].functions[func_idx];
	func_init_linear(func, in_min, in_max, out_min, out_max);
	
	err = config_storage_save(&cfg);
	if (err) {
		shell_error(sh, "Failed to save configuration");
		return err;
	}
	
	shell_print(sh, "Function %d configured as LINEAR: [%d,%d] → [%d,%d]",
		func_idx, in_min, in_max, out_min, out_max);
	shell_print(sh, "Run 'config reload' to apply changes");
	
	return 0;
}

/*
 * Configuration reload command
 */

static int cmd_config_reload(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
		shell_print(sh, "Configuration reloaded from storage");
	} else {
		shell_warn(sh, "Config reload callback not set");
	}
	
	return 0;
}

static int cmd_config_import(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_warn(sh, "Interactive import not yet implemented");
	shell_print(sh, "Use config_tool.py script for importing configurations");
	shell_print(sh, "Or manually set values using:");
	shell_print(sh, "  config midi_ch <1-16>");
	shell_print(sh, "  config select <0-15>");
	shell_print(sh, "  config save");
	
	return 0;
}

/*
 * Shell command registration
 */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_config,
	SHELL_CMD(show, NULL, "Show current configuration", cmd_config_show),
	SHELL_CMD(save, NULL, "Save configuration to flash", cmd_config_save),
	SHELL_CMD(restore, NULL, "Restore factory defaults", cmd_config_restore),
	SHELL_CMD(reload, NULL, "Reload configuration from storage", cmd_config_reload),
	SHELL_CMD_ARG(patch, NULL, "Show specific patch <0-15>", cmd_config_patch, 2, 0),
	SHELL_CMD_ARG(select, NULL, "Select active patch <0-15>", cmd_config_select_patch, 2, 0),
	SHELL_CMD(list, NULL, "List patches", cmd_config_list_patches),
	SHELL_CMD_ARG(midi_ch, NULL, "Set MIDI channel <1-16>", cmd_config_midi_ch, 2, 0),
	SHELL_CMD_ARG(accel_deadzone, NULL, "Set CC change threshold <0-127>", cmd_config_accel_deadzone, 2, 0),
	SHELL_CMD_ARG(scan_interval, NULL, "Set BLE scan interval <10-1000> ms", cmd_config_scan_interval, 2, 0),
	SHELL_CMD_ARG(avg_enable, NULL, "Enable running average <0|1>", cmd_config_avg_enable, 2, 0),
	SHELL_CMD_ARG(avg_depth, NULL, "Set average depth <3-10> samples", cmd_config_avg_depth, 2, 0),
	SHELL_CMD_ARG(export, NULL, "Export config [global | patch <0-15>]", cmd_config_export, 1, 2),
	SHELL_CMD(import, NULL, "Import config from JSON", cmd_config_import),
	SHELL_CMD(erase_all, NULL, "Erase all config (testing only)", cmd_config_erase_all),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_midi,
	SHELL_CMD(rx_stats, NULL, "Show MIDI RX statistics", cmd_midi_rx_stats),
	SHELL_CMD(rx_reset, NULL, "Reset MIDI RX statistics", cmd_midi_rx_reset),
	SHELL_CMD_ARG(program, NULL, "Get/set MIDI program [0-127]", cmd_midi_program, 1, 1),
	SHELL_CMD_ARG(send_rt, NULL, "Send MIDI real-time message <0xF8-0xFF>", cmd_midi_send_rt, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_topo,
	SHELL_CMD(show, NULL, "Show topology configuration", cmd_topo_show),
	/* SHELL_CMD_ARG(enable, NULL, "Enable virtual ports <0|1>", cmd_topo_enable, 2, 0), */ /* Legacy - topology always active */
	SHELL_CMD_ARG(config, NULL, "Configure topology <inst> <type> <accel> [func] [cc]", cmd_topo_config, 4, 2),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_func,
	SHELL_CMD_ARG(show, NULL, "Show function unit [idx]", cmd_func_show, 1, 1),
	SHELL_CMD_ARG(linear, NULL, "Configure LINEAR <idx> <in_min> <in_max> <out_min> <out_max>", cmd_func_linear, 6, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(config, &sub_config, "Configuration commands", NULL);
SHELL_CMD_REGISTER(midi, &sub_midi, "MIDI commands", NULL);
SHELL_CMD_REGISTER(topo, &sub_topo, "Topology commands", NULL);
SHELL_CMD_REGISTER(func, &sub_func, "Function unit commands", NULL);
SHELL_CMD_REGISTER(status, NULL, "Show system status", cmd_status);

/*
 * Public API
 */

int ui_interface_init(void)
{
	LOG_INF("UI interface initialized (Zephyr Shell)");
	return 0;
}

void ui_set_connected_devices(int count)
{
	connected_devices = count;
}

void ui_set_midi_output_active(bool active)
{
	midi_output_active = active;
}

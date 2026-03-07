/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_interface.h"
#include "config_storage.h"
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
	shell_print(sh, "Accelerometer:");
	shell_print(sh, "  Scale: [%d, %d, %d, %d, %d, %d]",
		cfg.global.accel_scale[0], cfg.global.accel_scale[1], cfg.global.accel_scale[2],
		cfg.global.accel_scale[3], cfg.global.accel_scale[4], cfg.global.accel_scale[5]);
	shell_print(sh, "Filters:");
	shell_print(sh, "  Running average: %s", cfg.global.running_average_enable ? "Enabled" : "Disabled");
	shell_print(sh, "  Average depth: %d samples", cfg.global.running_average_depth);
	
	uint8_t patch_idx = cfg.global.default_patch;
	if (patch_idx >= 16) patch_idx = 0;
	
	shell_print(sh, "\n--- PATCH SETTINGS (Patch %d) ---", patch_idx);
	shell_print(sh, "Name: %s", cfg.patches[patch_idx].patch_name);
	shell_print(sh, "MIDI:");
	shell_print(sh, "  Velocity curve: %d", cfg.patches[patch_idx].velocity_curve);
	shell_print(sh, "  CC mapping: [%d, %d, %d, %d, %d, %d]",
		cfg.patches[patch_idx].cc_mapping[0], cfg.patches[patch_idx].cc_mapping[1], cfg.patches[patch_idx].cc_mapping[2],
		cfg.patches[patch_idx].cc_mapping[3], cfg.patches[patch_idx].cc_mapping[4], cfg.patches[patch_idx].cc_mapping[5]);
	shell_print(sh, "LED:");
	shell_print(sh, "  Mode: %d", cfg.patches[patch_idx].led_mode);
	shell_print(sh, "Accelerometer:");
	shell_print(sh, "  Deadzone: %d", cfg.patches[patch_idx].accel_deadzone);
	shell_print(sh, "  Min CC: [%d, %d, %d, %d, %d, %d]",
		cfg.patches[patch_idx].accel_min[0], cfg.patches[patch_idx].accel_min[1],
		cfg.patches[patch_idx].accel_min[2], cfg.patches[patch_idx].accel_min[3],
		cfg.patches[patch_idx].accel_min[4], cfg.patches[patch_idx].accel_min[5]);
	shell_print(sh, "  Max CC: [%d, %d, %d, %d, %d, %d]",
		cfg.patches[patch_idx].accel_max[0], cfg.patches[patch_idx].accel_max[1],
		cfg.patches[patch_idx].accel_max[2], cfg.patches[patch_idx].accel_max[3],
		cfg.patches[patch_idx].accel_max[4], cfg.patches[patch_idx].accel_max[5]);
	shell_print(sh, "  Invert: 0x%02X", cfg.patches[patch_idx].accel_invert);
	
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
	if (patch_idx >= 16) patch_idx = 0;
	
	cfg.patches[patch_idx].cc_mapping[axis] = cc_num;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	const char *axis_names[] = {"X", "Y", "Z"};
	shell_print(sh, "%s-axis CC set to %d (patch %d setting)", axis_names[axis], cc_num, patch_idx);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

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
	if (patch_idx >= 16) patch_idx = 0;
	
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
	if (patch_idx >= 16) patch_idx = 0;
	
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
	if (patch_idx >= 16) patch_idx = 0;
	
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
	if (patch_idx >= 16) patch_idx = 0;
	
	cfg.patches[patch_idx].velocity_curve = (uint8_t)curve;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	shell_print(sh, "Velocity curve set to %d (patch %d setting)", curve, patch_idx);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

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
	shell_print(sh, "MIDI:");
	shell_print(sh, "  Velocity curve: %d", cfg.patches[patch_idx].velocity_curve);
	shell_print(sh, "  CC mapping: [%d, %d, %d, %d, %d, %d]",
		cfg.patches[patch_idx].cc_mapping[0], cfg.patches[patch_idx].cc_mapping[1],
		cfg.patches[patch_idx].cc_mapping[2], cfg.patches[patch_idx].cc_mapping[3],
		cfg.patches[patch_idx].cc_mapping[4], cfg.patches[patch_idx].cc_mapping[5]);
	shell_print(sh, "LED:");
	shell_print(sh, "  Mode: %d", cfg.patches[patch_idx].led_mode);
	shell_print(sh, "Accelerometer:");
	shell_print(sh, "  Deadzone: %d", cfg.patches[patch_idx].accel_deadzone);
	shell_print(sh, "  Min CC: [%d, %d, %d, %d, %d, %d]",
		cfg.patches[patch_idx].accel_min[0], cfg.patches[patch_idx].accel_min[1],
		cfg.patches[patch_idx].accel_min[2], cfg.patches[patch_idx].accel_min[3],
		cfg.patches[patch_idx].accel_min[4], cfg.patches[patch_idx].accel_min[5]);
	shell_print(sh, "  Max CC: [%d, %d, %d, %d, %d, %d]",
		cfg.patches[patch_idx].accel_max[0], cfg.patches[patch_idx].accel_max[1],
		cfg.patches[patch_idx].accel_max[2], cfg.patches[patch_idx].accel_max[3],
		cfg.patches[patch_idx].accel_max[4], cfg.patches[patch_idx].accel_max[5]);
	shell_print(sh, "  Invert: 0x%02X", cfg.patches[patch_idx].accel_invert);
	
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
	shell_print(sh, "        \"velocity_curve\": %d,", patch->velocity_curve);
	shell_print(sh, "        \"cc_mapping\": [%d, %d, %d, %d, %d, %d],",
		    patch->cc_mapping[0], patch->cc_mapping[1], patch->cc_mapping[2],
		    patch->cc_mapping[3], patch->cc_mapping[4], patch->cc_mapping[5]);
	shell_print(sh, "        \"led_mode\": %d,", patch->led_mode);
	shell_print(sh, "        \"accel_deadzone\": %d,", patch->accel_deadzone);
	shell_print(sh, "        \"accel_min\": [%d, %d, %d, %d, %d, %d],",
		    patch->accel_min[0], patch->accel_min[1], patch->accel_min[2],
		    patch->accel_min[3], patch->accel_min[4], patch->accel_min[5]);
	shell_print(sh, "        \"accel_max\": [%d, %d, %d, %d, %d, %d],",
		    patch->accel_max[0], patch->accel_max[1], patch->accel_max[2],
		    patch->accel_max[3], patch->accel_max[4], patch->accel_max[5]);
	shell_print(sh, "        \"accel_invert\": %d", patch->accel_invert);
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
		shell_print(sh, "      \"accel_scale\": [%d, %d, %d, %d, %d, %d],",
			    cfg.global.accel_scale[0], cfg.global.accel_scale[1], cfg.global.accel_scale[2],
			    cfg.global.accel_scale[3], cfg.global.accel_scale[4], cfg.global.accel_scale[5]);
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

static int cmd_config_import(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_warn(sh, "Interactive import not yet implemented");
	shell_print(sh, "Use config_tool.py script for importing configurations");
	shell_print(sh, "Or manually set values using:");
	shell_print(sh, "  config midi_ch <1-16>");
	shell_print(sh, "  config select <0-15>");
	shell_print(sh, "  config cc <x|y|z> <0-127>");
	shell_print(sh, "  config accel_min <0-5> <0-127>");
	shell_print(sh, "  config accel_max <0-5> <0-127>");
	shell_print(sh, "  config accel_invert <0-5> <0|1>");
	shell_print(sh, "  config velocity_curve <0-127>");
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
	SHELL_CMD_ARG(patch, NULL, "Show specific patch <0-15>", cmd_config_patch, 2, 0),
	SHELL_CMD_ARG(select, NULL, "Select active patch <0-15>", cmd_config_select_patch, 2, 0),
	SHELL_CMD(list, NULL, "List patches", cmd_config_list_patches),
	SHELL_CMD_ARG(midi_ch, NULL, "Set MIDI channel <1-16>", cmd_config_midi_ch, 2, 0),
	SHELL_CMD_ARG(cc, NULL, "Set CC mapping <x|y|z> <0-127>", cmd_config_cc, 3, 0),
	SHELL_CMD_ARG(accel_min, NULL, "Set axis min CC <0-5> <0-127>", cmd_config_accel_min, 3, 0),
	SHELL_CMD_ARG(accel_max, NULL, "Set axis max CC <0-5> <0-127>", cmd_config_accel_max, 3, 0),
	SHELL_CMD_ARG(accel_invert, NULL, "Invert axis <0-5> <0|1>", cmd_config_accel_invert, 3, 0),
	SHELL_CMD_ARG(velocity_curve, NULL, "Set velocity curve <0-127>", cmd_config_velocity_curve, 2, 0),
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

SHELL_CMD_REGISTER(config, &sub_config, "Configuration commands", NULL);
SHELL_CMD_REGISTER(midi, &sub_midi, "MIDI commands", NULL);
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

/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_interface.h"
#include "config_storage.h"
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
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
	
	struct config_data cfg;
	
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	shell_print(sh, "\n=== Configuration ===");
	shell_print(sh, "MIDI:");
	shell_print(sh, "  Channel: %d", cfg.midi_channel + 1);
	shell_print(sh, "  Velocity curve: %d", cfg.velocity_curve);
	shell_print(sh, "  CC mapping: [%d, %d, %d, %d, %d, %d]",
		cfg.cc_mapping[0], cfg.cc_mapping[1], cfg.cc_mapping[2],
		cfg.cc_mapping[3], cfg.cc_mapping[4], cfg.cc_mapping[5]);
	shell_print(sh, "BLE:");
	shell_print(sh, "  Max guitars: %d", cfg.max_guitars);
	shell_print(sh, "  Scan interval: %d ms", cfg.scan_interval_ms);
	shell_print(sh, "LED:");
	shell_print(sh, "  Brightness: %d", cfg.led_brightness);
	shell_print(sh, "  Mode: %d", cfg.led_mode);
	shell_print(sh, "Accelerometer:");
	shell_print(sh, "  Deadzone: %d", cfg.accel_deadzone);
	shell_print(sh, "  Scale: [%d, %d, %d, %d, %d, %d]",
		cfg.accel_scale[0], cfg.accel_scale[1], cfg.accel_scale[2],
		cfg.accel_scale[3], cfg.accel_scale[4], cfg.accel_scale[5]);
	
	return 0;
}

static int cmd_config_save(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	struct config_data cfg;
	
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
	
	struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.midi_channel = ch - 1;  /* 0-indexed internally */
	
	int ret = config_storage_save(&cfg);
	if (ret != 0) {
		shell_error(sh, "Error saving configuration (code: %d)", ret);
		return -1;
	}
	
	shell_print(sh, "MIDI channel set to %d", ch);
	
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
	
	struct config_data cfg;
	if (config_storage_load(&cfg) != 0) {
		shell_error(sh, "Error loading configuration");
		return -1;
	}
	
	cfg.cc_mapping[axis] = cc_num;
	
	if (config_storage_save(&cfg) != 0) {
		shell_error(sh, "Error saving configuration");
		return -1;
	}
	
	const char *axis_names[] = {"X", "Y", "Z"};
	shell_print(sh, "%s-axis CC set to %d", axis_names[axis], cc_num);
	
	/* Trigger config reload */
	if (ui_config_reload_callback) {
		ui_config_reload_callback();
	}
	
	return 0;
}

static int cmd_config_unlock_default(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	if (config_storage_unlock_default_write() != 0) {
		shell_error(sh, "DEFAULT writes disabled at compile time");
		shell_error(sh, "Enable CONFIG_CONFIG_ALLOW_DEFAULT_WRITE in prj.conf");
		return -1;
	}
	
	shell_warn(sh, "*** DEFAULT AREA UNLOCKED ***");
	shell_print(sh, "You can now use 'config write_default'");
	shell_print(sh, "Lock will auto-reset after write");
	
	return 0;
}

static int cmd_config_write_default(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_warn(sh, "WARNING: Writing to factory default area!");
	shell_warn(sh, "This should only be done during manufacturing.");
	
	struct config_data cfg;
	config_storage_get_hardcoded_defaults(&cfg);
	
	if (config_storage_write_default(&cfg) != 0) {
		shell_error(sh, "Error writing factory defaults");
		shell_error(sh, "Use 'config unlock_default' first");
		return -1;
	}
	
	shell_print(sh, "Factory defaults written successfully");
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

static int cmd_config_erase_all(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	
	shell_warn(sh, "*** WARNING: ERASE ALL CONFIGURATION STORAGE ***");
	shell_warn(sh, "This will erase DEFAULT, AREA_A, and AREA_B");
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

/*
 * Shell command registration
 */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_config,
	SHELL_CMD(show, NULL, "Show current configuration", cmd_config_show),
	SHELL_CMD(save, NULL, "Save configuration to flash", cmd_config_save),
	SHELL_CMD(restore, NULL, "Restore factory defaults", cmd_config_restore),
	SHELL_CMD_ARG(midi_ch, NULL, "Set MIDI channel <1-16>", cmd_config_midi_ch, 2, 0),
	SHELL_CMD_ARG(cc, NULL, "Set CC mapping <x|y|z> <0-127>", cmd_config_cc, 3, 0),
	SHELL_CMD(unlock_default, NULL, "Unlock DEFAULT area (dev only)", cmd_config_unlock_default),
	SHELL_CMD(write_default, NULL, "Write factory defaults (mfg only)", cmd_config_write_default),
	SHELL_CMD(erase_all, NULL, "Erase all config (testing only)", cmd_config_erase_all),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_midi,
	SHELL_CMD(rx_stats, NULL, "Show MIDI RX statistics", cmd_midi_rx_stats),
	SHELL_CMD(rx_reset, NULL, "Reset MIDI RX statistics", cmd_midi_rx_reset),
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

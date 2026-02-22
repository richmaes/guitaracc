/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_interface.h"
#include "config_storage.h"
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ui_interface, LOG_LEVEL_DBG);

#define CMD_BUFFER_SIZE 128
#define PROMPT "GuitarAcc> "

static const struct device *ui_uart_dev;
static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t cmd_buffer_pos = 0;
static bool echo_enabled = true;

/* System status */
static int connected_devices = 0;
static bool midi_output_active = false;

/* Configuration reload callback */
void (*ui_config_reload_callback)(void) = NULL;

/* Forward declarations */
static void cmd_help(void);
static void cmd_status(void);
static void cmd_echo(const char *args);
static void cmd_config(const char *args);
static void process_command(const char *cmd);
static void print_prompt(void);

/**
 * @brief Send string to UI UART
 */
static void ui_send_string(const char *str)
{
	if (!ui_uart_dev) {
		return;
	}
	
	while (*str) {
		uart_poll_out(ui_uart_dev, *str++);
	}
}

void ui_print(const char *fmt, ...)
{
	char buffer[256];
	va_list args;
	
	va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	
	ui_send_string(buffer);
}

static void print_prompt(void)
{
	ui_send_string("\r\n");
	ui_send_string(PROMPT);
}

static void cmd_help(void)
{
	ui_print("\r\nAvailable commands:\r\n");
	ui_print("  help           - Show this help message\r\n");
	ui_print("  status         - Show system status\r\n");
	ui_print("  echo           - Toggle echo mode (on/off)\r\n");
	ui_print("  clear          - Clear screen\r\n");
	ui_print("  config show              - Show current configuration\r\n");
	ui_print("  config save              - Save configuration to flash\r\n");
	ui_print("  config restore           - Restore factory defaults\r\n");
	ui_print("  config midi_ch <1-16>    - Set MIDI channel\r\n");
	ui_print("  config cc <x|y|z> <0-127> - Set CC number for axis\r\n");
}

static void cmd_status(void)
{
	
	/* Show config storage info */
	enum config_area active_area;
	uint32_t sequence;
	if (config_storage_get_info(&active_area, &sequence) == 0) {
		const char *area_name = (active_area == CONFIG_AREA_A) ? "A" : "B";
		ui_print("Config area: %s (seq=%u)\r\n", area_name, sequence);
	}
	ui_print("\r\n=== GuitarAcc Basestation Status ===\r\n");
	ui_print("Connected devices: %d\r\n", connected_devices);
	ui_print("MIDI output: %s\r\n", midi_output_active ? "Active" : "Inactive");
	ui_print("Echo mode: %s\r\n", echo_enabled ? "On" : "Off");
}

static void cmd_echo(const char *args)
{
	/* Skip leading spaces */
	while (*args == ' ') {
		args++;
	}
	
	if (strcmp(args, "on") == 0) {
		echo_enabled = true;
		ui_print("\r\nEcho enabled\r\n");
	} else if (strcmp(args, "off") == 0) {
		echo_enabled = false;
		ui_print("\r\nEcho disabled\r\n");
	} else {
		ui_print("\r\nUsage: echo on|off\r\n");
	}
}

static void cmd_clear(void)
{
	/* Send VT100 clear screen and cursor home */
	ui_send_string("\033[2J\033[H");
}

static void cmd_config(const char *args)
{
	/* Skip leading spaces */
	while (*args == ' ') {
		args++;
	}
	
	if (strncmp(args, "show", 4) == 0) {
		static struct config_data cfg;
		if (config_storage_load(&cfg) == 0) {
			ui_print("\r\n=== Configuration ===\r\n");
			
			ui_print("\r\n--- GLOBAL SETTINGS ---\r\n");
			ui_print("Active patch: %d\r\n", cfg.global.default_patch);
			ui_print("MIDI:\r\n");
			ui_print("  Channel: %d\r\n", cfg.global.midi_channel + 1);
			ui_print("BLE:\r\n");
			ui_print("  Max guitars: %d\r\n", cfg.global.max_guitars);
			ui_print("  Scan interval: %d ms\r\n", cfg.global.scan_interval_ms);
			ui_print("LED:\r\n");
			ui_print("  Brightness: %d\r\n", cfg.global.led_brightness);
			ui_print("Accelerometer:\r\n");
			ui_print("  Scale: [%d, %d, %d, %d, %d, %d]\r\n",
				cfg.global.accel_scale[0], cfg.global.accel_scale[1], cfg.global.accel_scale[2],
				cfg.global.accel_scale[3], cfg.global.accel_scale[4], cfg.global.accel_scale[5]);
			ui_print("Filters:\r\n");
			ui_print("  Running average: %s\r\n", cfg.global.running_average_enable ? "Enabled" : "Disabled");
			ui_print("  Average depth: %d samples\r\n", cfg.global.running_average_depth);
			
			uint8_t patch_idx = cfg.global.default_patch;
			if (patch_idx >= 127) patch_idx = 0;
			
			ui_print("\r\n--- PATCH SETTINGS (Patch %d) ---\r\n", patch_idx);
			ui_print("Name: %s\r\n", cfg.patches[patch_idx].patch_name);
			ui_print("MIDI:\r\n");
			ui_print("  Velocity curve: %d\r\n", cfg.patches[patch_idx].velocity_curve);
			ui_print("  CC mapping: [%d, %d, %d, %d, %d, %d]\r\n",
				cfg.patches[patch_idx].cc_mapping[0], cfg.patches[patch_idx].cc_mapping[1], cfg.patches[patch_idx].cc_mapping[2],
				cfg.patches[patch_idx].cc_mapping[3], cfg.patches[patch_idx].cc_mapping[4], cfg.patches[patch_idx].cc_mapping[5]);
			ui_print("LED:\r\n");
			ui_print("  Mode: %d\r\n", cfg.patches[patch_idx].led_mode);
			ui_print("Accelerometer:\r\n");
			ui_print("  Deadzone: %d\r\n", cfg.patches[patch_idx].accel_deadzone);
			ui_print("  Min CC: [%d, %d, %d, %d, %d, %d]\r\n",
				cfg.patches[patch_idx].accel_min[0], cfg.patches[patch_idx].accel_min[1],
				cfg.patches[patch_idx].accel_min[2], cfg.patches[patch_idx].accel_min[3],
				cfg.patches[patch_idx].accel_min[4], cfg.patches[patch_idx].accel_min[5]);
			ui_print("  Max CC: [%d, %d, %d, %d, %d, %d]\r\n",
				cfg.patches[patch_idx].accel_max[0], cfg.patches[patch_idx].accel_max[1],
				cfg.patches[patch_idx].accel_max[2], cfg.patches[patch_idx].accel_max[3],
				cfg.patches[patch_idx].accel_max[4], cfg.patches[patch_idx].accel_max[5]);
			ui_print("  Invert: 0x%02X\r\n", cfg.patches[patch_idx].accel_invert);
		} else {
			ui_print("\r\nError loading configuration\r\n");
		}
	} else if (strncmp(args, "save", 4) == 0) {
		static struct config_data cfg;
		if (config_storage_load(&cfg) == 0) {
			if (config_storage_save(&cfg) == 0) {
				ui_print("\r\nConfiguration saved to flash\r\n");
			} else {
				ui_print("\r\nError saving configuration\r\n");
			}
		}
	} else if (strncmp(args, "restore", 7) == 0) {
		if (config_storage_restore_defaults() == 0) {
			ui_print("\r\nFactory defaults restored\r\n");
		} else {
			ui_print("\r\nError restoring defaults\r\n");
		}
	} else if (strncmp(args, "midi_ch", 7) == 0) {
		/* Set MIDI channel: config midi_ch <1-16> */
		int ch = atoi(args + 7);
		if (ch >= 1 && ch <= 16) {
			static struct config_data cfg;
			if (config_storage_load(&cfg) == 0) {
				cfg.global.midi_channel = ch - 1;  /* 0-indexed internally */
				int ret = config_storage_save(&cfg);
				if (ret == 0) {
					ui_print("\r\nMIDI channel set to %d (global setting)\r\n", ch);
					/* Trigger config reload */
					if (ui_config_reload_callback) {
						ui_config_reload_callback();
					}
				} else {
					ui_print("\r\nError saving configuration (code: %d)\r\n", ret);
				}
			}
		} else {
			ui_print("\r\nInvalid channel (1-16)\r\n");
		}
	} else if (strncmp(args, "cc", 2) == 0) {
		/* Set CC mapping: config cc <axis> <cc_num> */
		/* axis: x=0, y=1, z=2, roll=3, pitch=4, yaw=5 */
		const char *axis_str = args + 2;
		while (*axis_str == ' ') axis_str++;
		
		int axis = -1;
		if (*axis_str == 'x' || *axis_str == 'X') axis = 0;
		else if (*axis_str == 'y' || *axis_str == 'Y') axis = 1;
		else if (*axis_str == 'z' || *axis_str == 'Z') axis = 2;
		
		if (axis >= 0) {
			axis_str++;
			while (*axis_str == ' ') axis_str++;
			int cc_num = atoi(axis_str);
			
			if (cc_num >= 0 && cc_num <= 127) {
				static struct config_data cfg;
				if (config_storage_load(&cfg) == 0) {
					uint8_t patch_idx = cfg.global.default_patch;
					if (patch_idx >= 127) patch_idx = 0;
					
					cfg.patches[patch_idx].cc_mapping[axis] = cc_num;
					if (config_storage_save(&cfg) == 0) {
						const char *axis_names[] = {"X", "Y", "Z", "Roll", "Pitch", "Yaw"};
						ui_print("\r\n%s-axis CC set to %d (patch %d setting)\r\n", axis_names[axis], cc_num, patch_idx);
						/* Trigger config reload */
						if (ui_config_reload_callback) {
							ui_config_reload_callback();
						}
					} else {
						ui_print("\r\nError saving configuration\r\n");
					}
				}
			} else {
				ui_print("\r\nInvalid CC number (0-127)\r\n");
			}
		} else {
			ui_print("\r\nUsage: config cc <x|y|z> <cc_num>\r\n");
		}
	} else if (strncmp(args, "erase_all", 9) == 0) {
		ui_print("\r\n*** WARNING: ERASE ALL CONFIGURATION STORAGE ***\r\n");
		ui_print("This will erase DEFAULT, AREA_A, and AREA_B\r\n");
		ui_print("Device will use hardcoded defaults on next boot\r\n");
		ui_print("This command is for TESTING ONLY!\r\n");
		ui_print("\r\nType 'ERASE' to confirm: ");
		/* Note: This is just a warning - actual confirmation would need input handling */
		
		if (config_storage_erase_all() == 0) {
			ui_print("\r\n\r\nAll configuration erased successfully\r\n");
			ui_print("*** REBOOT REQUIRED ***\r\n");
			ui_print("Use 'reboot' command or power cycle the device\r\n");
		} else {
			ui_print("\r\n\r\nError erasing configuration storage\r\n");
		}
	} else {
		ui_print("\r\nUsage: config show|save|restore|midi_ch|cc|unlock_default|write_default|erase_all\r\n");
	}
}

static void process_command(const char *cmd)
{
	/* Skip leading spaces */
	while (*cmd == ' ') {
		cmd++;
	}
	
	/* Empty command */
	if (*cmd == '\0') {
		print_prompt();
		return;
	}
	
	/* Extract command word */
	const char *space = strchr(cmd, ' ');
	size_t cmd_len = space ? (size_t)(space - cmd) : strlen(cmd);
	const char *args = space ? space + 1 : cmd + cmd_len;
	
	if (strncmp(cmd, "help", cmd_len) == 0 && cmd_len == 4) {
		cmd_help();
	} else if (strncmp(cmd, "status", cmd_len) == 0 && cmd_len == 6) {
		cmd_status();
	} else if (strncmp(cmd, "echo", cmd_len) == 0 && cmd_len == 4) {
		cmd_echo(args);
	} else if (strncmp(cmd, "clear", cmd_len) == 0 && cmd_len == 5) {
		cmd_clear();
	} else if (strncmp(cmd, "config", cmd_len) == 0 && cmd_len == 6) {
		cmd_config(args);
	} else {
		ui_print("\r\nUnknown command: %.*s\r\n", cmd_len, cmd);
		ui_print("Type 'help' for available commands\r\n");
	}
	
	print_prompt();
}

void ui_interface_process_char(char c)
{
	/* Handle backspace */
	if (c == '\b' || c == 127) {
		if (cmd_buffer_pos > 0) {
			cmd_buffer_pos--;
			if (echo_enabled) {
				ui_send_string("\b \b");  /* Backspace, space, backspace */
			}
		}
		return;
	}
	
	/* Handle newline/carriage return */
	if (c == '\r' || c == '\n') {
		if (echo_enabled) {
			ui_send_string("\r\n");
		}
		
		/* Null-terminate command */
		cmd_buffer[cmd_buffer_pos] = '\0';
		
		/* Process command */
		if (cmd_buffer_pos > 0) {
			process_command(cmd_buffer);
		} else {
			print_prompt();
		}
		
		/* Reset buffer */
		cmd_buffer_pos = 0;
		return;
	}
	
	/* Handle printable characters */
	if (c >= 32 && c <= 126) {
		if (cmd_buffer_pos < CMD_BUFFER_SIZE - 1) {
			cmd_buffer[cmd_buffer_pos++] = c;
			if (echo_enabled) {
				uart_poll_out(ui_uart_dev, c);
			}
		}
	}
}

void ui_interface_update_status(int connected_count, bool midi_active)
{
	connected_devices = connected_count;
	midi_output_active = midi_active;
}

int ui_interface_init(const struct device *uart_dev)
{
	if (!uart_dev) {
		LOG_ERR("Invalid UART device");
		return -EINVAL;
	}
	
	ui_uart_dev = uart_dev;
	cmd_buffer_pos = 0;
	
	/* Send welcome message */
	ui_send_string("\r\n");
	ui_send_string("========================================\r\n");
	ui_send_string("  GuitarAcc Basestation v1.0\r\n");
	ui_send_string("  Type 'help' for available commands\r\n");
	ui_send_string("========================================\r\n");
	print_prompt();
	
	LOG_INF("UI interface initialized");
	
	return 0;
}

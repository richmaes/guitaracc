/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_interface.h"
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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

/* Forward declarations */
static void cmd_help(void);
static void cmd_status(void);
static void cmd_echo(const char *args);
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
	ui_print("  help    - Show this help message\r\n");
	ui_print("  status  - Show system status\r\n");
	ui_print("  echo    - Toggle echo mode (on/off)\r\n");
	ui_print("  clear   - Clear screen\r\n");
}

static void cmd_status(void)
{
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

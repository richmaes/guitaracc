/*
 * Virtual Port Infrastructure Implementation
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "virtual_ports.h"
#include <string.h>

/* ========================================
 * HELPER FUNCTIONS
 * ======================================== */

/**
 * @brief Clamp a value to MIDI range
 */
static inline uint8_t clamp_to_midi(int16_t value)
{
	if (value < MIDI_MIN_VALUE) {
		return MIDI_MIN_VALUE;
	}
	if (value > MIDI_MAX_VALUE) {
		return MIDI_MAX_VALUE;
	}
	return (uint8_t)value;
}

/**
 * @brief Validate port number
 */
static inline bool is_valid_port(uint8_t port_num)
{
	return port_num < MAX_VIRTUAL_PORTS;
}

/* ========================================
 * PUBLIC API
 * ======================================== */

void vport_init(struct virtual_port_system *vps, enum vport_mixer_type default_mixer)
{
	if (!vps) {
		return;
	}
	
	memset(vps, 0, sizeof(*vps));
	vps->default_mixer_type = default_mixer;
	
	/* Initialize all ports with default mixer */
	for (int i = 0; i < MAX_VIRTUAL_PORTS; i++) {
		vps->ports[i].mixer_type = default_mixer;
	}
}

void vport_reset_all(struct virtual_port_system *vps)
{
	if (!vps) {
		return;
	}
	
	for (int i = 0; i < MAX_VIRTUAL_PORTS; i++) {
		vps->ports[i].value = 0;
		vps->ports[i].input_count = 0;
		/* Keep mixer_type - it's part of configuration */
	}
}

int vport_write(struct virtual_port_system *vps, uint8_t port_num, int16_t value)
{
	if (!vps || !is_valid_port(port_num)) {
		return -1;
	}
	
	struct virtual_port *port = &vps->ports[port_num];
	
	/* First write to this port this sample? */
	if (port->input_count == 0) {
		port->value = value;
	} else {
		/* Combine based on mixer type */
		switch (port->mixer_type) {
		case MIXER_PASSTHROUGH:
			/* Overwrite with latest value */
			port->value = value;
			break;
			
		case MIXER_SUM:
			/* Sum all inputs */
			port->value += value;
			break;
			
		case MIXER_AVERAGE:
			/* Running average */
			port->value += value;
			break;
			
		case MIXER_MAX:
			/* Maximum value */
			if (value > port->value) {
				port->value = value;
			}
			break;
			
		case MIXER_MIN:
			/* Minimum value */
			if (value < port->value) {
				port->value = value;
			}
			break;
			
		default:
			/* Unknown mixer type - treat as passthrough */
			port->value = value;
			break;
		}
	}
	
	port->input_count++;
	return 0;
}

uint8_t vport_read(const struct virtual_port_system *vps, uint8_t port_num)
{
	if (!vps || !is_valid_port(port_num)) {
		return 0;
	}
	
	const struct virtual_port *port = &vps->ports[port_num];
	
	/* Apply final mixing calculation if needed */
	int16_t final_value = port->value;
	
	if (port->mixer_type == MIXER_AVERAGE && port->input_count > 1) {
		/* Divide by input count for average */
		final_value = port->value / port->input_count;
	}
	
	return clamp_to_midi(final_value);
}

int16_t vport_read_raw(const struct virtual_port_system *vps, uint8_t port_num)
{
	if (!vps || !is_valid_port(port_num)) {
		return 0;
	}
	
	const struct virtual_port *port = &vps->ports[port_num];
	
	/* Apply final mixing calculation if needed */
	int16_t final_value = port->value;
	
	if (port->mixer_type == MIXER_AVERAGE && port->input_count > 1) {
		/* Divide by input count for average */
		final_value = port->value / port->input_count;
	}
	
	/* Return raw value without MIDI clamping */
	return final_value;
}

int vport_set_mixer(struct virtual_port_system *vps, uint8_t port_num, 
                    enum vport_mixer_type mixer_type)
{
	if (!vps || !is_valid_port(port_num)) {
		return -1;
	}
	
	vps->ports[port_num].mixer_type = mixer_type;
	return 0;
}

uint8_t vport_get_input_count(const struct virtual_port_system *vps, uint8_t port_num)
{
	if (!vps || !is_valid_port(port_num)) {
		return 0;
	}
	
	return vps->ports[port_num].input_count;
}

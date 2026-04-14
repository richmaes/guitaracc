/*
 * Virtual Port Infrastructure
 * Signal routing fabric for accelerometer to MIDI mapping
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef VIRTUAL_PORTS_H
#define VIRTUAL_PORTS_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================
 * CONSTANTS
 * ======================================== */

#define MAX_VIRTUAL_PORTS       18   /* Virtual port array size (6 instances × 3 ports) */
#define MIDI_MIN_VALUE          0
#define MIDI_MAX_VALUE          127

/* ========================================
 * MIXER TYPES
 * ======================================== */

/**
 * @brief Virtual port mixer algorithms
 * 
 * Determines how multiple inputs are combined when writing to a virtual port.
 */
enum vport_mixer_type {
	MIXER_PASSTHROUGH = 0,  /* Single input, no mixing */
	MIXER_SUM,              /* Sum all inputs (clamped to 0-127) */
	MIXER_AVERAGE,          /* Average all inputs */
	MIXER_WEIGHTED_AVG,     /* Weighted average (not yet implemented) */
	MIXER_MAX,              /* Maximum of all inputs */
	MIXER_MIN,              /* Minimum of all inputs */
};

/* ========================================
 * DATA STRUCTURES
 * ======================================== */

/**
 * @brief Virtual port runtime state
 * 
 * Not persisted - computed fresh on each sample.
 */
struct virtual_port {
	int16_t value;           /* Current value (can exceed MIDI range during mixing) */
	uint8_t input_count;     /* Number of inputs that wrote to this port this sample */
	uint8_t mixer_type;      /* Mixer algorithm (enum vport_mixer_type) */
};

/**
 * @brief Virtual port system state
 * 
 * Contains all virtual ports and runtime state.
 */
struct virtual_port_system {
	struct virtual_port ports[MAX_VIRTUAL_PORTS];
	uint8_t default_mixer_type;  /* Default mixer for all ports */
};

/* ========================================
 * API FUNCTIONS
 * ======================================== */

/**
 * @brief Initialize the virtual port system
 * 
 * @param vps Pointer to virtual port system
 * @param default_mixer Default mixer type for all ports
 */
void vport_init(struct virtual_port_system *vps, enum vport_mixer_type default_mixer);

/**
 * @brief Reset all virtual ports for new sample
 * 
 * Call this at the start of each processing cycle before writing any values.
 * 
 * @param vps Pointer to virtual port system
 */
void vport_reset_all(struct virtual_port_system *vps);

/**
 * @brief Write a value to a virtual port
 * 
 * If multiple values are written to the same port, they are combined
 * according to the port's mixer type.
 * 
 * @param vps Pointer to virtual port system
 * @param port_num Virtual port number (0 to MAX_VIRTUAL_PORTS-1)
 * @param value Value to write (0-127 typically, but can be out of range)
 * @return 0 on success, negative on error
 */
int vport_write(struct virtual_port_system *vps, uint8_t port_num, int16_t value);

/**
 * @brief Read the current value from a virtual port
 * 
 * Returns the mixed value after all writes have been completed.
 * Value is clamped to MIDI range (0-127).
 * 
 * @param vps Pointer to virtual port system
 * @param port_num Virtual port number (0 to MAX_VIRTUAL_PORTS-1)
 * @return Current port value (0-127), or 0 on error
 */
uint8_t vport_read(const struct virtual_port_system *vps, uint8_t port_num);

/**
 * @brief Read the raw value from a virtual port without MIDI clamping
 * 
 * Returns the mixed value without clamping to MIDI range.
 * Used for passing values between function units.
 * 
 * @param vps Pointer to virtual port system
 * @param port_num Virtual port number (0 to MAX_VIRTUAL_PORTS-1)
 * @return Current port value (unclamped), or 0 on error
 */
int16_t vport_read_raw(const struct virtual_port_system *vps, uint8_t port_num);

/**
 * @brief Set mixer type for a specific virtual port
 * 
 * @param vps Pointer to virtual port system
 * @param port_num Virtual port number (0 to MAX_VIRTUAL_PORTS-1)
 * @param mixer_type Mixer algorithm to use
 * @return 0 on success, negative on error
 */
int vport_set_mixer(struct virtual_port_system *vps, uint8_t port_num, 
                    enum vport_mixer_type mixer_type);

/**
 * @brief Get the number of inputs that wrote to a port
 * 
 * Useful for debugging and validation.
 * 
 * @param vps Pointer to virtual port system
 * @param port_num Virtual port number (0 to MAX_VIRTUAL_PORTS-1)
 * @return Number of inputs, or 0 on error
 */
uint8_t vport_get_input_count(const struct virtual_port_system *vps, uint8_t port_num);

#endif /* VIRTUAL_PORTS_H */

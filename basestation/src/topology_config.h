/*
 * Topology Configuration
 * Fixed topology patterns for signal routing
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TOPOLOGY_CONFIG_H
#define TOPOLOGY_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

/* ========================================
 * CONSTANTS
 * ======================================== */

#define MAX_TOPOLOGY_INSTANCES  6    /* Concurrent topology instances per patch */
#define MAX_FUNCTION_UNITS      8    /* Function processing blocks */
#define MAX_ACCEL_SOURCES       6    /* X, Y, Z, Roll, Pitch, Yaw */
#define MAX_MIDI_OUTPUTS        6    /* Configurable CC numbers per patch */
#define NUM_TOPOLOGY_TYPES      4    /* T1, T2, T3, T4 */

/* ========================================
 * TOPOLOGY TYPES
 * ======================================== */

/**
 * @brief Fixed topology patterns
 * 
 * Each topology defines a specific signal routing pattern.
 */
enum topology_type {
	TOPO_DISABLED = 0,      /* Instance not configured/inactive */
	TOPO_T1 = 1,            /* Accel → VP → Func → VP → MIDI */
	TOPO_T2 = 2,            /* Accel₁ + Accel₂ → VP → Func → VP → MIDI */
	TOPO_T3 = 3,            /* Accel → VP → Func → VP → (MIDI₁, MIDI₂) */
	TOPO_T4 = 4,            /* Accel₁ + Accel₂ → VP → Func₁ → VP → MIDI₁ */
	                        /*                              ↳ Func₂ → VP → MIDI₂ */
};

/* ========================================
 * DATA STRUCTURES
 * ======================================== */

/**
 * @brief Configuration for one topology instance
 * 
 * A topology instance represents one active signal path.
 * The valid fields depend on the topology type.
 */
struct topology_instance {
	uint8_t topology_type;      /* enum topology_type */
	uint8_t accel_inputs[2];    /* Which accel axis/axes (0-5) */
	uint8_t func_units[2];      /* Which function unit(s) to use (0-7) */
	uint8_t midi_outputs[2];    /* Which MIDI CC(s) for output (0-127) */
	uint8_t enabled;            /* Instance active flag */
	uint8_t reserved[3];        /* Padding for alignment */
} __attribute__((packed));

/**
 * @brief Per-patch topology configuration
 * 
 * Contains all topology instances and is stored per patch.
 * Each of 16 patches can have a different configuration.
 */
struct patch_topology_config {
	struct topology_instance topologies[MAX_TOPOLOGY_INSTANCES];
	uint8_t default_mixer_type;     /* Default mixing algorithm */
	uint8_t reserved[7];             /* Padding to 8-byte boundary */
} __attribute__((packed));

/* ========================================
 * VALIDATION
 * ======================================== */

/**
 * @brief Validate a topology instance configuration
 * 
 * Checks that all parameters are within valid ranges for the topology type.
 * 
 * @param topo Pointer to topology instance
 * @return true if valid, false otherwise
 */
bool topology_validate(const struct topology_instance *topo);

/**
 * @brief Get a human-readable name for a topology type
 * 
 * @param type Topology type enum value
 * @return String name, or "Unknown" for invalid types
 */
const char *topology_get_name(enum topology_type type);

/**
 * @brief Get the number of accelerometer inputs for a topology type
 * 
 * @param type Topology type enum value
 * @return Number of accel inputs (1-2), or 0 for invalid/disabled
 */
uint8_t topology_get_accel_input_count(enum topology_type type);

/**
 * @brief Get the number of function units for a topology type
 * 
 * @param type Topology type enum value
 * @return Number of function units (1-2), or 0 for invalid/disabled
 */
uint8_t topology_get_function_count(enum topology_type type);

/**
 * @brief Get the number of MIDI outputs for a topology type
 * 
 * @param type Topology type enum value
 * @return Number of MIDI outputs (1-2), or 0 for invalid/disabled
 */
uint8_t topology_get_midi_output_count(enum topology_type type);

/**
 * @brief Get the number of virtual ports used by a topology type
 * 
 * @param type Topology type enum value
 * @return Number of VPs (2 for T1/T2, 3 for T3/T4), or 0 for invalid/disabled
 */
uint8_t topology_get_vport_count(enum topology_type type);

/**
 * @brief Get the human-readable name for a sensor source
 * 
 * @param source_idx Sensor source index (0-5)
 * @return String name (ACCEL_X, ACCEL_Y, ACCEL_Z, GYRO_ROLL, GYRO_PITCH, GYRO_YAW)
 */
const char *topology_get_sensor_name(uint8_t source_idx);

/**
 * @brief Initialize a topology instance with defaults
 * 
 * @param topo Pointer to topology instance
 * @param type Topology type to configure
 */
void topology_init_default(struct topology_instance *topo, enum topology_type type);

/**
 * @brief Initialize patch topology config with defaults
 * 
 * Sets up all 6 axes with T1 topology (simple linear processing).
 * 
 * @param config Pointer to patch topology config
 */
void topology_patch_init_default(struct patch_topology_config *config);

#endif /* TOPOLOGY_CONFIG_H */

/*
 * Topology Processor
 * Executes configured topology instances
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef TOPOLOGY_PROCESSOR_H
#define TOPOLOGY_PROCESSOR_H

#include <stdint.h>
#include "virtual_ports.h"
#include "topology_config.h"
#include "function_units.h"

/* ========================================
 * DATA STRUCTURES
 * ======================================== */

/**
 * @brief Complete processing context
 * 
 * Contains all state needed to process topology instances.
 */
struct topology_processor {
	struct virtual_port_system vport_system;
	struct function_unit functions[MAX_FUNCTION_UNITS];
	struct patch_topology_config *current_patch;
	int16_t accel_values[MAX_ACCEL_SOURCES];  /* Current accel readings */
	uint8_t midi_outputs[MAX_MIDI_OUTPUTS];    /* Resulting MIDI CC values */
};

/* ========================================
 * API FUNCTIONS
 * ======================================== */

/**
 * @brief Initialize the topology processor
 * 
 * @param proc Pointer to processor structure
 * @param patch_config Pointer to patch configuration to use
 */
void topo_proc_init(struct topology_processor *proc, 
                    struct patch_topology_config *patch_config);

/**
 * @brief Set accelerometer input values
 * 
 * Call this before processing to update accelerometer readings.
 * 
 * @param proc Pointer to processor structure
 * @param accel_data Array of 6 accelerometer values (X,Y,Z,Roll,Pitch,Yaw)
 */
void topo_proc_set_accel_inputs(struct topology_processor *proc, 
                                const int16_t accel_data[MAX_ACCEL_SOURCES]);

/**
 * @brief Apply global scale/offset calibration to sensor values
 * 
 * This is a helper function that applies global calibration to raw sensor
 * readings. It reads scale and offset from global configuration and produces
 * calibrated values suitable for topology processing.
 * 
 * This implements the CALIBRATION LAYER from the architecture:
 *   calibrated = ((raw - offset) * 127) / scale
 * 
 * Recommended usage: Call this once per sample before topo_proc_execute()
 * to pre-process all sensor values with global calibration.
 * 
 * @param raw_values Array of 6 raw sensor readings (milli-g or raw units)
 * @param calibrated_values Output array of 6 calibrated values (int16_t)
 * @param scale Array of 6 scale values from config.global.accel_scale[]
 * @param offset Array of 6 offset values from config.global.accel_offset[]
 */
void topo_proc_apply_global_calibration(const int16_t raw_values[MAX_ACCEL_SOURCES],
                                        int16_t calibrated_values[MAX_ACCEL_SOURCES],
                                        const int16_t scale[MAX_ACCEL_SOURCES],
                                        const int16_t offset[MAX_ACCEL_SOURCES]);

/**
 * @brief Process all enabled topology instances
 * 
 * Executes all enabled topologies in the current patch and produces
 * MIDI output values.
 * 
 * @param proc Pointer to processor structure
 * @return 0 on success, negative on error
 */
int topo_proc_execute(struct topology_processor *proc);

/**
 * @brief Get MIDI output value for a specific CC number
 * 
 * Call this after topo_proc_execute() to retrieve results.
 * 
 * @param proc Pointer to processor structure
 * @param cc_index Index into MIDI outputs array (0-5)
 * @return MIDI value (0-127), or 0 if index invalid
 */
uint8_t topo_proc_get_midi_output(const struct topology_processor *proc, 
                                  uint8_t cc_index);

/**
 * @brief Get all MIDI output values
 * 
 * @param proc Pointer to processor structure
 * @param output_buffer Buffer to receive MIDI values (must be at least MAX_MIDI_OUTPUTS)
 */
void topo_proc_get_all_midi_outputs(const struct topology_processor *proc, 
                                    uint8_t *output_buffer);

/**
 * @brief Configure a function unit for the processor
 * 
 * @param proc Pointer to processor structure
 * @param func_index Function unit index (0-7)
 * @param func Function unit configuration
 * @return 0 on success, negative on error
 */
int topo_proc_set_function(struct topology_processor *proc, 
                           uint8_t func_index, 
                           const struct function_unit *func);

#endif /* TOPOLOGY_PROCESSOR_H */

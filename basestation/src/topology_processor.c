/*
 * Topology Processor Implementation
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "topology_processor.h"
#include <string.h>

/* ========================================
 * PRIVATE HELPERS
 * ======================================== */

/**
 * @brief Virtual port allocation for each topology instance
 * 
 * We allocate virtual ports dynamically based on topology instance index:
 * - Instance 0: VP[0], VP[1], VP[2] (up to 3 VPs for T4)
 * - Instance 1: VP[3], VP[4], VP[5]
 * - Instance 2: VP[6], VP[7], VP[8]
 * etc.
 */
static inline uint8_t get_vport_base(uint8_t instance_idx)
{
	return instance_idx * 3;
}

/**
 * @brief Process a single topology instance
 */
static int process_topology_instance(struct topology_processor *proc,
                                     const struct topology_instance *topo,
                                     uint8_t instance_idx)
{
	if (!topo->enabled || topo->topology_type == TOPO_DISABLED) {
		return 0;
	}
	
	uint8_t vp_base = get_vport_base(instance_idx);
	struct virtual_port_system *vps = &proc->vport_system;
	
	/* Process based on topology type */
	switch (topo->topology_type) {
	
	case TOPO_T1: {
		/* Accel → VP[0] → Func → VP[1] → MIDI */
		uint8_t accel_idx = topo->accel_inputs[0];
		uint8_t func_idx = topo->func_units[0];
		uint8_t midi_idx = topo->midi_outputs[0] - 16; /* CC 16-21 → index 0-5 */
		
		if (accel_idx >= MAX_ACCEL_SOURCES || func_idx >= MAX_FUNCTION_UNITS) {
			return -1;
		}
		
		/* Write accel to VP[0] */
		vport_write(vps, vp_base + 0, proc->accel_values[accel_idx]);
		
		/* Process through function - use raw value */
		int16_t func_input = vport_read_raw(vps, vp_base + 0);
		int16_t func_output = func_process(&proc->functions[func_idx], func_input);
		
		/* Write to VP[1] */
		vport_write(vps, vp_base + 1, func_output);
		
		/* Read and store to MIDI output - now clamp to MIDI range */
		if (midi_idx < MAX_MIDI_OUTPUTS) {
			proc->midi_outputs[midi_idx] = vport_read(vps, vp_base + 1);
		}
		break;
	}
	
	case TOPO_T2: {
		/* Accel₁ + Accel₂ → VP[0] (mix) → Func → VP[1] → MIDI */
		uint8_t accel_idx0 = topo->accel_inputs[0];
		uint8_t accel_idx1 = topo->accel_inputs[1];
		uint8_t func_idx = topo->func_units[0];
		uint8_t midi_idx = topo->midi_outputs[0] - 16;
		
		if (accel_idx0 >= MAX_ACCEL_SOURCES || accel_idx1 >= MAX_ACCEL_SOURCES ||
		    func_idx >= MAX_FUNCTION_UNITS) {
			return -1;
		}
		
		/* Write both accels to VP[0] (mixing) */
		vport_write(vps, vp_base + 0, proc->accel_values[accel_idx0]);
		vport_write(vps, vp_base + 0, proc->accel_values[accel_idx1]);
		
		/* Process through function - use raw value */
		int16_t func_input = vport_read_raw(vps, vp_base + 0);
		int16_t func_output = func_process(&proc->functions[func_idx], func_input);
		
		/* Write to VP[1] */
		vport_write(vps, vp_base + 1, func_output);
		
		/* Read and store to MIDI output - clamp to MIDI range */
		if (midi_idx < MAX_MIDI_OUTPUTS) {
			proc->midi_outputs[midi_idx] = vport_read(vps, vp_base + 1);
		}
		break;
	}
	
	case TOPO_T3: {
		/* Accel → VP[0] → Func → VP[1] → (MIDI₁, MIDI₂) */
		uint8_t accel_idx = topo->accel_inputs[0];
		uint8_t func_idx = topo->func_units[0];
		uint8_t midi_idx0 = topo->midi_outputs[0] - 16;
		uint8_t midi_idx1 = topo->midi_outputs[1] - 16;
		
		if (accel_idx >= MAX_ACCEL_SOURCES || func_idx >= MAX_FUNCTION_UNITS) {
			return -1;
		}
		
		/* Write accel to VP[0] */
		vport_write(vps, vp_base + 0, proc->accel_values[accel_idx]);
		
		/* Process through function - use raw value */
		int16_t func_input = vport_read_raw(vps, vp_base + 0);
		int16_t func_output = func_process(&proc->functions[func_idx], func_input);
		
		/* Write to VP[1] */
		vport_write(vps, vp_base + 1, func_output);
		
		/* Read and store to both MIDI outputs (fan-out) - clamp to MIDI range */
		uint8_t midi_value = vport_read(vps, vp_base + 1);
		if (midi_idx0 < MAX_MIDI_OUTPUTS) {
			proc->midi_outputs[midi_idx0] = midi_value;
		}
		if (midi_idx1 < MAX_MIDI_OUTPUTS) {
			proc->midi_outputs[midi_idx1] = midi_value;
		}
		break;
	}
	
	case TOPO_T4: {
		/* Accel₁ + Accel₂ → VP[0] → Func₁ → VP[1] → MIDI₁ */
		/*                                  ↳ Func₂ → VP[2] → MIDI₂ */
		uint8_t accel_idx0 = topo->accel_inputs[0];
		uint8_t accel_idx1 = topo->accel_inputs[1];
		uint8_t func_idx0 = topo->func_units[0];
		uint8_t func_idx1 = topo->func_units[1];
		uint8_t midi_idx0 = topo->midi_outputs[0] - 16;
		uint8_t midi_idx1 = topo->midi_outputs[1] - 16;
		
		if (accel_idx0 >= MAX_ACCEL_SOURCES || accel_idx1 >= MAX_ACCEL_SOURCES ||
		    func_idx0 >= MAX_FUNCTION_UNITS || func_idx1 >= MAX_FUNCTION_UNITS) {
			return -1;
		}
		
		/* Write both accels to VP[0] (mixing) */
		vport_write(vps, vp_base + 0, proc->accel_values[accel_idx0]);
		vport_write(vps, vp_base + 0, proc->accel_values[accel_idx1]);
		
		/* Process through first function - use raw value */
		int16_t mixed_input = vport_read_raw(vps, vp_base + 0);
		int16_t func0_output = func_process(&proc->functions[func_idx0], mixed_input);
		
		/* Write to VP[1] and output to MIDI₁ - clamp to MIDI range */
		vport_write(vps, vp_base + 1, func0_output);
		if (midi_idx0 < MAX_MIDI_OUTPUTS) {
			proc->midi_outputs[midi_idx0] = vport_read(vps, vp_base + 1);
		}
		
		/* Also process through second function (cascaded) */
		int16_t func1_output = func_process(&proc->functions[func_idx1], func0_output);
		
		/* Write to VP[2] and output to MIDI₂ - clamp to MIDI range */
		vport_write(vps, vp_base + 2, func1_output);
		if (midi_idx1 < MAX_MIDI_OUTPUTS) {
			proc->midi_outputs[midi_idx1] = vport_read(vps, vp_base + 2);
		}
		break;
	}
	
	default:
		return -1;
	}
	
	return 0;
}

/* ========================================
 * PUBLIC API
 * ======================================== */

void topo_proc_init(struct topology_processor *proc, 
                    struct patch_topology_config *patch_config)
{
	if (!proc) {
		return;
	}
	
	memset(proc, 0, sizeof(*proc));
	proc->current_patch = patch_config;
	
	/* Initialize virtual port system */
	enum vport_mixer_type mixer = MIXER_AVERAGE;
	if (patch_config) {
		mixer = (enum vport_mixer_type)patch_config->default_mixer_type;
	}
	vport_init(&proc->vport_system, mixer);
	
	/* Initialize all function units with defaults */
	for (int i = 0; i < MAX_FUNCTION_UNITS; i++) {
		func_init(&proc->functions[i], FUNC_LINEAR);
	}
}

void topo_proc_set_accel_inputs(struct topology_processor *proc, 
                                const int16_t accel_data[MAX_ACCEL_SOURCES])
{
	if (!proc || !accel_data) {
		return;
	}
	
	memcpy(proc->accel_values, accel_data, sizeof(proc->accel_values));
}

int topo_proc_execute(struct topology_processor *proc)
{
	if (!proc || !proc->current_patch) {
		return -1;
	}
	
	/* Reset virtual ports for new processing cycle */
	vport_reset_all(&proc->vport_system);
	
	/* Clear MIDI outputs */
	memset(proc->midi_outputs, 0, sizeof(proc->midi_outputs));
	
	/* Process each enabled topology instance */
	for (int i = 0; i < MAX_TOPOLOGY_INSTANCES; i++) {
		const struct topology_instance *topo = &proc->current_patch->topologies[i];
		
		int result = process_topology_instance(proc, topo, i);
		if (result < 0) {
			/* Log error but continue processing other instances */
			continue;
		}
	}
	
	return 0;
}

uint8_t topo_proc_get_midi_output(const struct topology_processor *proc, 
                                  uint8_t cc_index)
{
	if (!proc || cc_index >= MAX_MIDI_OUTPUTS) {
		return 0;
	}
	
	return proc->midi_outputs[cc_index];
}

void topo_proc_get_all_midi_outputs(const struct topology_processor *proc, 
                                    uint8_t *output_buffer)
{
	if (!proc || !output_buffer) {
		return;
	}
	
	memcpy(output_buffer, proc->midi_outputs, MAX_MIDI_OUTPUTS);
}

int topo_proc_set_function(struct topology_processor *proc, 
                           uint8_t func_index, 
                           const struct function_unit *func)
{
	if (!proc || func_index >= MAX_FUNCTION_UNITS || !func) {
		return -1;
	}
	
	memcpy(&proc->functions[func_index], func, sizeof(struct function_unit));
	return 0;
}

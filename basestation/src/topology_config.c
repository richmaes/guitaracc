/*
 * Topology Configuration Implementation
 * 
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "topology_config.h"
#include <string.h>

/* ========================================
 * VALIDATION
 * ======================================== */

bool topology_validate(const struct topology_instance *topo)
{
	if (!topo) {
		return false;
	}
	
	/* Disabled is always valid */
	if (topo->topology_type == TOPO_DISABLED) {
		return true;
	}
	
	/* Check topology type is within range */
	if (topo->topology_type > NUM_TOPOLOGY_TYPES) {
		return false;
	}
	
	/* Validate based on topology type */
	uint8_t expected_accel_inputs = topology_get_accel_input_count(topo->topology_type);
	uint8_t expected_func_count = topology_get_function_count(topo->topology_type);
	uint8_t expected_midi_outputs = topology_get_midi_output_count(topo->topology_type);
	
	/* Check accelerometer inputs */
	for (int i = 0; i < expected_accel_inputs; i++) {
		if (topo->accel_inputs[i] >= MAX_ACCEL_SOURCES) {
			return false;
		}
	}
	
	/* Check function units */
	for (int i = 0; i < expected_func_count; i++) {
		if (topo->func_units[i] >= MAX_FUNCTION_UNITS) {
			return false;
		}
	}
	
	/* Check MIDI outputs */
	for (int i = 0; i < expected_midi_outputs; i++) {
		if (topo->midi_outputs[i] > 127) {
			return false;
		}
	}
	
	return true;
}

const char *topology_get_name(enum topology_type type)
{
	switch (type) {
	case TOPO_DISABLED:
		return "Disabled";
	case TOPO_T1:
		return "T1 (Simple Linear)";
	case TOPO_T2:
		return "T2 (Two Inputs Merged)";
	case TOPO_T3:
		return "T3 (Fan-out)";
	case TOPO_T4:
		return "T4 (Cascaded)";
	default:
		return "Unknown";
	}
}

uint8_t topology_get_accel_input_count(enum topology_type type)
{
	switch (type) {
	case TOPO_T1:
	case TOPO_T3:
		return 1;
	case TOPO_T2:
	case TOPO_T4:
		return 2;
	default:
		return 0;
	}
}

uint8_t topology_get_function_count(enum topology_type type)
{
	switch (type) {
	case TOPO_T1:
	case TOPO_T2:
	case TOPO_T3:
		return 1;
	case TOPO_T4:
		return 2;
	default:
		return 0;
	}
}

uint8_t topology_get_midi_output_count(enum topology_type type)
{
	switch (type) {
	case TOPO_T1:
	case TOPO_T2:
		return 1;
	case TOPO_T3:
	case TOPO_T4:
		return 2;
	default:
		return 0;
	}
}

void topology_init_default(struct topology_instance *topo, enum topology_type type)
{
	if (!topo) {
		return;
	}
	
	memset(topo, 0, sizeof(*topo));
	topo->topology_type = type;
	topo->enabled = (type != TOPO_DISABLED) ? 1 : 0;
	
	/* Set reasonable defaults based on type */
	switch (type) {
	case TOPO_T1:
		/* Single accel, single function, single MIDI */
		topo->accel_inputs[0] = 0;  /* X-axis */
		topo->func_units[0] = 0;    /* Function 0 */
		topo->midi_outputs[0] = 16; /* CC 16 */
		break;
		
	case TOPO_T2:
		/* Two accel inputs, single function, single MIDI */
		topo->accel_inputs[0] = 0;  /* X-axis */
		topo->accel_inputs[1] = 1;  /* Y-axis */
		topo->func_units[0] = 0;    /* Function 0 */
		topo->midi_outputs[0] = 16; /* CC 16 */
		break;
		
	case TOPO_T3:
		/* Single accel, single function, two MIDI outputs */
		topo->accel_inputs[0] = 0;  /* X-axis */
		topo->func_units[0] = 0;    /* Function 0 */
		topo->midi_outputs[0] = 16; /* CC 16 */
		topo->midi_outputs[1] = 17; /* CC 17 */
		break;
		
	case TOPO_T4:
		/* Two accel inputs, two functions, two MIDI outputs */
		topo->accel_inputs[0] = 0;  /* X-axis */
		topo->accel_inputs[1] = 1;  /* Y-axis */
		topo->func_units[0] = 0;    /* Function 0 */
		topo->func_units[1] = 1;    /* Function 1 */
		topo->midi_outputs[0] = 16; /* CC 16 */
		topo->midi_outputs[1] = 17; /* CC 17 */
		break;
		
	default:
		break;
	}
}

void topology_patch_init_default(struct patch_topology_config *config)
{
	if (!config) {
		return;
	}
	
	memset(config, 0, sizeof(*config));
	config->default_mixer_type = 2; /* MIXER_AVERAGE */
	
	/* Configure all 6 axes with T1 topology */
	for (int i = 0; i < 6 && i < MAX_TOPOLOGY_INSTANCES; i++) {
		topology_init_default(&config->topologies[i], TOPO_T1);
		config->topologies[i].accel_inputs[0] = i;     /* X, Y, Z, Roll, Pitch, Yaw */
		config->topologies[i].func_units[0] = i;       /* Each gets own function unit */
		config->topologies[i].midi_outputs[0] = 16 + i; /* CC 16-21 */
		config->topologies[i].enabled = 1;
	}
}

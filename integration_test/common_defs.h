/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * Common Definitions
 * Shared between client and basestation
 */

#ifndef COMMON_DEFS_H
#define COMMON_DEFS_H

#include <stdint.h>

/* Acceleration data structure: X, Y, Z in milli-g */
struct accel_data {
	int16_t x;  /* X-axis in milli-g */
	int16_t y;  /* Y-axis in milli-g */
	int16_t z;  /* Z-axis in milli-g */
} __attribute__((packed));

#endif /* COMMON_DEFS_H */

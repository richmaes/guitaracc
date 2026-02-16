/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MOTION_LOGIC_H
#define MOTION_LOGIC_H

#include <stdint.h>
#include <stdbool.h>

/* Acceleration data structure: X, Y, Z in milli-g */
struct accel_data {
	int16_t x;  /* X-axis in milli-g */
	int16_t y;  /* Y-axis in milli-g */
	int16_t z;  /* Z-axis in milli-g */
} __attribute__((packed));

/* Motion detection configuration */
#define MOTION_THRESHOLD 0.5     /* m/s² threshold for motion detection */

/* Enable/disable running average filter (define to enable) */
#define ENABLE_RUNNING_AVERAGE 1

/* Running average depth (adjustable 3-10) */
#define RUNNING_AVERAGE_DEPTH 5

/* Spike limiter configuration */
#define SPIKE_LIMIT_MILLI_G 500  /* Maximum allowed change per sample (0.5g) */

/**
 * @brief Initialize spike limiter
 * 
 * Sets the initial reference values for spike detection. Call this
 * before using apply_spike_limiter().
 * 
 * @param initial Initial acceleration data
 */
void spike_limiter_init(const struct accel_data *initial);

/**
 * @brief Apply spike limiter to acceleration data
 * 
 * Limits the change in acceleration between samples to prevent
 * sudden spikes from noise or sensor errors. Each axis is independently
 * limited to SPIKE_LIMIT_MILLI_G change per sample.
 * 
 * @param raw Raw acceleration data from sensor
 * @param limited Output limited acceleration data
 */
void apply_spike_limiter(const struct accel_data *raw, struct accel_data *limited);

/**
 * @brief Initialize running average filter
 * 
 * Resets the circular buffers and state for the running average filter.
 * Must be called before using apply_running_average().
 */
void running_average_init(void);

/**
 * @brief Apply running average filter
 * 
 * Applies a running average filter to smooth accelerometer data.
 * The depth is configurable via RUNNING_AVERAGE_DEPTH (3-10 samples).
 * Uses separate circular buffers for each axis.
 * 
 * @param input Input acceleration data
 * @param output Output smoothed acceleration data
 */
void apply_running_average(const struct accel_data *input, struct accel_data *output);

/**
 * @brief Convert acceleration from m/s² to milli-g
 * 
 * Converts SI units (meters per second squared) to milli-g units
 * where 1g = 9.81 m/s². Result is clamped to int16_t range.
 * 
 * @param m_s2 Acceleration in m/s²
 * @return Acceleration in milli-g (±32767 range)
 */
int16_t convert_to_milli_g(double m_s2);

/**
 * @brief Convert XYZ acceleration from m/s² to milli-g
 * 
 * Converts all three axes from SI units to milli-g and stores
 * in the provided accel_data structure.
 * 
 * @param x X-axis acceleration in m/s²
 * @param y Y-axis acceleration in m/s²
 * @param z Z-axis acceleration in m/s²
 * @param data Output structure to store converted values
 */
void convert_accel_to_milli_g(double x, double y, double z, struct accel_data *data);

/**
 * @brief Calculate magnitude of 3D acceleration vector
 * 
 * Computes the Euclidean magnitude: sqrt(x² + y² + z²)
 * 
 * @param x X-axis acceleration in m/s²
 * @param y Y-axis acceleration in m/s²
 * @param z Z-axis acceleration in m/s²
 * @return Magnitude in m/s²
 */
double calculate_magnitude(double x, double y, double z);

/**
 * @brief Detect if motion exceeds threshold
 * 
 * Detects motion by measuring deviation from 1g gravity (9.81 m/s²).
 * If the magnitude differs from gravity by more than MOTION_THRESHOLD,
 * motion is detected.
 * 
 * @param x X-axis acceleration in m/s²
 * @param y Y-axis acceleration in m/s²
 * @param z Z-axis acceleration in m/s²
 * @return true if motion detected, false otherwise
 */
bool detect_motion(double x, double y, double z);

/**
 * @brief Compare two acceleration data structures
 * 
 * Checks if any axis (x, y, or z) differs between current and previous data.
 * Used to avoid sending redundant BLE notifications.
 * 
 * @param current Current acceleration data
 * @param previous Previous acceleration data
 * @return true if data changed, false if unchanged
 */
bool accel_data_changed(const struct accel_data *current, const struct accel_data *previous);

/**
 * @brief Detect movement exceeding threshold
 * 
 * Checks if acceleration has changed by more than the specified threshold
 * on any axis. Useful for detecting significant motion while filtering out
 * noise and minor vibrations.
 * 
 * @param current Current acceleration data in milli-g
 * @param previous Previous acceleration data in milli-g
 * @param threshold_milli_g Minimum change in milli-g to detect (e.g., 50 for 0.05g)
 * @return true if any axis changed by more than threshold, false otherwise
 */
bool detect_movement_threshold(const struct accel_data *current,
                                const struct accel_data *previous,
                                int16_t threshold_milli_g);

#endif /* MOTION_LOGIC_H */

/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CONFIG_STORAGE_H_
#define CONFIG_STORAGE_H_

#include <zephyr/kernel.h>
#include <stdint.h>

/**
 * @brief Configuration Storage Module
 * 
 * This module manages configuration data in external QSPI flash using a 
 * redundant storage scheme with three areas:
 * 
 * - DEFAULT: Factory default configuration (read-only after initial write)
 * - AREA_A: Primary active storage area
 * - AREA_B: Secondary active storage area (ping-pong with A)
 * 
 * The system ping-pongs between A and B, always using the most recent valid
 * configuration (determined by sequence number and hash validation).
 */

/* Configuration data structure version */
#define CONFIG_VERSION 1

/* Maximum configuration data size (excluding header) */
#define CONFIG_DATA_MAX_SIZE 4096

/* SHA256 hash size */
#define CONFIG_HASH_SIZE 32

/**
 * @brief Configuration header structure
 * 
 * Stored at the beginning of each configuration area to track version,
 * sequence number, and hash for validation.
 */
struct config_header {
	uint32_t magic;           /* Magic number: 0x47544143 ("GTAC") */
	uint32_t version;         /* Configuration structure version */
	uint32_t sequence;        /* Sequence number for ping-pong selection */
	uint32_t data_size;       /* Size of configuration data */
	uint8_t  hash[CONFIG_HASH_SIZE]; /* SHA256 hash of data */
	uint32_t crc32;           /* CRC32 of header (excluding this field) */
} __packed;

/**
 * @brief Global configuration structure
 * 
 * System-level settings that apply across all patches.
 * These settings are typically hardware or user preference related.
 */
struct global_config {
	/* Patch management */
	uint8_t default_patch;         /* Active patch index (0-126) */
	
	/* MIDI configuration */
	uint8_t midi_channel;          /* MIDI channel (0-15) */
	
	/* BLE configuration */
	uint8_t max_guitars;           /* Maximum connected guitars (1-4) */
	uint8_t scan_interval_ms;      /* BLE scan interval */
	
	/* LED configuration */
	uint8_t led_brightness;        /* LED brightness (0-255) */
	
	/* Accelerometer mapping */
	int16_t accel_scale[6];        /* Scaling factors for each axis */
	
	/* Filter configuration */
	uint8_t running_average_enable; /* Enable running average filter (0/1) */
	uint8_t running_average_depth;  /* Running average depth (3-10) */
	
	/* Reserved for future global settings */
	uint8_t reserved[33];          /* Future expansion (33 bytes for 4-byte alignment) */
} __packed;

/**
 * @brief Patch configuration structure
 * 
 * Patch-specific settings that can vary between different
 * performances, songs, or sound designs.
 */
struct patch_config {
	/* MIDI mapping configuration */
	uint8_t velocity_curve;        /* Velocity curve type (0-3) */
	uint8_t cc_mapping[6];         /* CC numbers for 6 axes */
	
	/* LED configuration */
	uint8_t led_mode;              /* LED mode (0-3) */
	
	/* Accelerometer mapping */
	int16_t accel_deadzone;        /* Deadzone threshold */
	uint8_t accel_min[6];          /* Minimum CC value for each axis (0-127) */
	uint8_t accel_max[6];          /* Maximum CC value for each axis (0-127) */
	uint8_t accel_invert;          /* Inversion bitfield: bit 0-5 for axes 0-5 */
	
	/* Patch metadata */
	char patch_name[32];           /* Patch name (null-terminated) */
	
	/* Reserved for future patch settings */
	uint8_t reserved[53];          /* Future expansion (53 bytes for 4-byte alignment) */
} __packed;

/**
 * @brief Combined configuration data structure
 * 
 * Contains both global and patch-specific settings.
 * This structure is stored in flash and survives power cycles.
 * 
 * NOTE: Supports up to 127 patches. The active patch is selected
 * via global.default_patch index.
 */
struct config_data {
	struct global_config global;   /* Global system settings */
	struct patch_config patches[127]; /* Array of 127 patches */
	
	/* Reserved for future use */
	uint8_t reserved[32];          /* Additional expansion space */
} __packed;

/* Compile-time assertion: nRF5340 flash requires 4-byte aligned writes */
BUILD_ASSERT(sizeof(struct config_data) % 4 == 0, 
	     "config_data size must be 4-byte aligned for flash writes");

/**
 * @brief Storage area identifiers
 */
enum config_area {
	CONFIG_AREA_A = 0,        /* Active storage A */
	CONFIG_AREA_B = 1,        /* Active storage B */
};

/**
 * @brief Initialize configuration storage system
 * 
 * Initializes QSPI flash driver, validates all storage areas, and loads
 * the most recent valid configuration. If no valid configuration exists,
 * loads from DEFAULT area. If DEFAULT is invalid, uses hardcoded defaults.
 * 
 * @return 0 on success, negative errno on failure
 */
int config_storage_init(void);

/**
 * @brief Save configuration data
 * 
 * Writes configuration to the inactive area (ping-pong), increments sequence
 * number, calculates hash, and updates header. The new area becomes active.
 * 
 * @param data Pointer to configuration data structure
 * @return 0 on success, negative errno on failure
 */
int config_storage_save(const struct config_data *data);

/**
 * @brief Load current configuration data
 * 
 * Retrieves the currently active configuration (most recent valid area).
 * 
 * @param data Pointer to buffer for configuration data
 * @return 0 on success, negative errno on failure
 */
int config_storage_load(struct config_data *data);

/**
 * @brief Restore factory default configuration
 * 
 * Loads hardcoded default configuration and saves it to active storage.
 * 
 * @return 0 on success, negative errno on failure
 */
int config_storage_restore_defaults(void);

/**
 * @brief Erase all configuration storage areas (testing only)
 * 
 * Returns information about sequence numbers, active area, and validation
 * status for debugging purposes.
 * 
 * @param active_area Pointer to store active area identifier
 * @param sequence Pointer to store current sequence number
 * @return 0 on success, negative errno on failure
 */
int config_storage_get_info(enum config_area *active_area, uint32_t *sequence);

/**
 * @brief Erase all configuration storage areas (testing only)
 * 
 * Erases DEFAULT, AREA_A, and AREA_B to simulate a brand new device.
 * Used for testing initialization with no stored configuration.
 * 
 * WARNING: This permanently erases all stored configuration!
 * Only use for testing and development.
 * 
 * @return 0 on success, negative errno on failure
 */
int config_storage_erase_all(void);

/**
 * @brief Get default configuration values (hardcoded)
 * 
 * Returns hardcoded default configuration used when no valid stored
 * configuration exists.
 * 
 * @param data Pointer to buffer for default configuration
 */
void config_storage_get_hardcoded_defaults(struct config_data *data);

#endif /* CONFIG_STORAGE_H_ */

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
 * @brief Configuration data structure
 * 
 * Add your configuration parameters here. This structure is stored
 * in QSPI flash and survives power cycles.
 */
struct config_data {
	/* MIDI mapping configuration */
	uint8_t midi_channel;          /* MIDI channel (0-15) */
	uint8_t velocity_curve;        /* Velocity curve type (0-3) */
	uint8_t cc_mapping[6];         /* CC numbers for 6 axes */
	
	/* BLE configuration */
	uint8_t max_guitars;           /* Maximum connected guitars (1-4) */
	uint8_t scan_interval_ms;      /* BLE scan interval */
	
	/* LED configuration */
	uint8_t led_brightness;        /* LED brightness (0-255) */
	uint8_t led_mode;              /* LED mode (0-3) */
	
	/* Accelerometer mapping */
	int16_t accel_deadzone;        /* Deadzone threshold */
	int16_t accel_scale[6];        /* Scaling factors for each axis */
	
	/* Reserved for future use */
	uint8_t reserved[130];         /* Padded to 156 bytes for 4-byte alignment */
} __packed;

/* Compile-time assertion: nRF5340 flash requires 4-byte aligned writes */
BUILD_ASSERT(sizeof(struct config_data) % 4 == 0, 
	     "config_data size must be 4-byte aligned for flash writes");

/**
 * @brief Storage area identifiers
 */
enum config_area {
	CONFIG_AREA_DEFAULT = 0,  /* Factory default (read-only) */
	CONFIG_AREA_A = 1,        /* Active storage A */
	CONFIG_AREA_B = 2,        /* Active storage B */
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
 * Loads configuration from DEFAULT area and saves it to active storage.
 * 
 * @return 0 on success, negative errno on failure
 */
int config_storage_restore_defaults(void);

/**
 * @brief Write factory default configuration (test application only)
 * 
 * Writes configuration to the DEFAULT area. This should only be called
 * from the test application during manufacturing/setup.
 * 
 * WARNING: This function should NOT be called from normal application code.
 * 
 * @param data Pointer to factory default configuration
 * @return 0 on success, negative errno on failure
 */
int config_storage_write_default(const struct config_data *data);

/**
 * @brief Get statistics about configuration storage
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
 * @brief Unlock DEFAULT area for writing (development/manufacturing only)
 * 
 * This function must be called before config_storage_write_default() will
 * succeed. It provides runtime protection against accidental DEFAULT writes.
 * The unlock is automatically cleared after a successful write.
 * 
 * Requirements:
 * - CONFIG_CONFIG_ALLOW_DEFAULT_WRITE must be enabled at compile time
 * - This function must be called immediately before write_default()
 * 
 * WARNING: Only use during development and manufacturing setup!
 * 
 * @return 0 on success, -EPERM if compile-time option disabled, -EACCES if not initialized
 */
int config_storage_unlock_default_write(void);

/**
 * @brief Check if DEFAULT area writes are currently enabled
 * 
 * @return true if DEFAULT writes are unlocked and compile-time enabled, false otherwise
 */
bool config_storage_is_default_write_enabled(void);

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

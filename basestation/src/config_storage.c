/*
 * Copyright (c) 2026 GuitarAcc Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config_storage.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>
#include <string.h>

#if defined(CONFIG_MBEDTLS_SHA256_C)
#include <mbedtls/sha256.h>
#endif

LOG_MODULE_REGISTER(config_storage, LOG_LEVEL_DBG);

/* Magic number for configuration header */
#define CONFIG_MAGIC 0x47544143  /* "GTAC" */

/* Internal flash device (nRF5340 has 1MB internal flash) */
#define FLASH_DEVICE DEVICE_DT_GET(DT_CHOSEN(zephyr_flash_controller))

/* Memory layout in internal flash using Partition Manager
 * The settings_storage partition is defined in pm_static.yml at 0xFC000
 * This matches the partition manager output and avoids code conflicts
 */
#define CONFIG_FLASH_OFFSET  0x000FC000  /* From pm_static.yml */
#define CONFIG_STORAGE_SIZE  0x00008000  /* 32KB */

/* Ping-pong storage with two 16KB areas (no DEFAULT area) */
#define CONFIG_AREA_A_OFFSET   (CONFIG_FLASH_OFFSET + 0x0000)  /* +0KB */
#define CONFIG_AREA_A_SIZE     0x00004000  /* 16KB */
#define CONFIG_AREA_B_OFFSET   (CONFIG_FLASH_OFFSET + 0x4000)  /* +16KB */
#define CONFIG_AREA_B_SIZE     0x00004000  /* 16KB */

/* Flash page size for nRF5340 internal flash */
#define FLASH_PAGE_SIZE 4096

/* Current configuration state */
static struct config_data current_config;
static enum config_area active_area = CONFIG_AREA_A;
static uint32_t current_sequence = 0;
static bool initialized = false;

/* Flash device handle */
static const struct device *flash_dev;

/**
 * @brief Calculate SHA256 hash of data
 */
static int calculate_hash(const void *data, size_t len, uint8_t *hash)
{
#if defined(CONFIG_MBEDTLS_SHA256_C)
	mbedtls_sha256_context ctx;
	
	mbedtls_sha256_init(&ctx);
	int ret = mbedtls_sha256_starts(&ctx, 0); /* 0 = SHA256, not SHA224 */
	if (ret != 0) {
		mbedtls_sha256_free(&ctx);
		return -EIO;
	}
	
	ret = mbedtls_sha256_update(&ctx, data, len);
	if (ret != 0) {
		mbedtls_sha256_free(&ctx);
		return -EIO;
	}
	
	ret = mbedtls_sha256_finish(&ctx, hash);
	mbedtls_sha256_free(&ctx);
	
	return (ret == 0) ? 0 : -EIO;
#else
	LOG_ERR("SHA256 not available, using simple checksum");
	/* Fallback: simple checksum (not cryptographically secure) */
	uint32_t sum = 0;
	const uint8_t *bytes = (const uint8_t *)data;
	for (size_t i = 0; i < len; i++) {
		sum += bytes[i];
	}
	memset(hash, 0, CONFIG_HASH_SIZE);
	memcpy(hash, &sum, sizeof(sum));
	return 0;
#endif
}

/**
 * @brief Verify hash of data
 */
static bool verify_hash(const void *data, size_t len, const uint8_t *expected_hash)
{
	uint8_t calculated_hash[CONFIG_HASH_SIZE];
	
	if (calculate_hash(data, len, calculated_hash) != 0) {
		return false;
	}
	
	return memcmp(calculated_hash, expected_hash, CONFIG_HASH_SIZE) == 0;
}

/**
 * @brief Calculate CRC32 of header (excluding crc32 field)
 */
static uint32_t calculate_header_crc(const struct config_header *header)
{
	size_t crc_len = offsetof(struct config_header, crc32);
	return crc32_ieee((const uint8_t *)header, crc_len);
}

/**
 * @brief Get flash offset for configuration area
 */
static off_t get_area_offset(enum config_area area)
{
	switch (area) {
	case CONFIG_AREA_A:
		return CONFIG_AREA_A_OFFSET;
	case CONFIG_AREA_B:
		return CONFIG_AREA_B_OFFSET;
	default:
		return -EINVAL;
	}
}

/**
 * @brief Read configuration area from flash
 */
static int read_area(enum config_area area, struct config_header *header,
		     struct config_data *data)
{
	off_t offset = get_area_offset(area);
	if (offset < 0) {
		return offset;
	}
	
	/* Read header */
	int ret = flash_read(flash_dev, offset, header, sizeof(*header));
	if (ret != 0) {
		LOG_ERR("Failed to read header from area %d: %d", area, ret);
		return ret;
	}
	
	/* Validate magic number */
	if (header->magic != CONFIG_MAGIC) {
		LOG_WRN("Invalid magic in area %d: 0x%08x", area, header->magic);
		return -EINVAL;
	}
	
	/* Validate header CRC */
	uint32_t calculated_crc = calculate_header_crc(header);
	if (calculated_crc != header->crc32) {
		LOG_WRN("Header CRC mismatch in area %d", area);
		return -EINVAL;
	}
	
	/* Validate data size */
	if (header->data_size > CONFIG_DATA_MAX_SIZE) {
		LOG_ERR("Invalid data size in area %d: %u", area, header->data_size);
		return -EINVAL;
	}
	
	/* Read data */
	ret = flash_read(flash_dev, offset + sizeof(*header), data,
			 sizeof(*data));
	if (ret != 0) {
		LOG_ERR("Failed to read data from area %d: %d", area, ret);
		return ret;
	}
	
	/* Verify data hash */
	if (!verify_hash(data, sizeof(*data), header->hash)) {
		LOG_ERR("Data hash mismatch in area %d", area);
		return -EINVAL;
	}
	
	LOG_INF("Successfully read area %d (seq=%u)", area, header->sequence);
	return 0;
}

/**
 * @brief Write configuration area to flash
 */
static int write_area(enum config_area area, const struct config_data *data,
		      uint32_t sequence)
{
	off_t offset = get_area_offset(area);
	if (offset < 0) {
		return offset;
	}
	
	/* Build header */
	struct config_header header = {
		.magic = CONFIG_MAGIC,
		.version = CONFIG_VERSION,
		.sequence = sequence,
		.data_size = sizeof(*data),
	};
	
	/* Calculate data hash */
	int ret = calculate_hash(data, sizeof(*data), header.hash);
	if (ret != 0) {
		LOG_ERR("Failed to calculate hash: %d", ret);
		return ret;
	}
	
	/* Calculate header CRC */
	header.crc32 = calculate_header_crc(&header);
	
	/* Erase flash page */
	LOG_DBG("Erasing flash at offset 0x%08lx, size 0x%x", (long)offset, FLASH_PAGE_SIZE);
	ret = flash_erase(flash_dev, offset, FLASH_PAGE_SIZE);
	if (ret != 0) {
		LOG_ERR("Failed to erase area %d at 0x%08lx: %d", area, (long)offset, ret);
		return ret;
	}
	
	/* Write header */
	LOG_DBG("Writing header to offset 0x%08lx", (long)offset);
	ret = flash_write(flash_dev, offset, &header, sizeof(header));
	if (ret != 0) {
		LOG_ERR("Failed to write header to area %d at 0x%08lx: %d", area, (long)offset, ret);
		return ret;
	}
	
	/* Write data */
	LOG_DBG("Writing data to offset 0x%08lx", (long)(offset + sizeof(header)));
	ret = flash_write(flash_dev, offset + sizeof(header), data, sizeof(*data));
	if (ret != 0) {
		LOG_ERR("Failed to write data to area %d at 0x%08lx: %d", area, (long)(offset + sizeof(header)), ret);
		return ret;
	}
	
	LOG_INF("Successfully wrote area %d (seq=%u)", area, sequence);
	return 0;
}

void config_storage_get_hardcoded_defaults(struct config_data *data)
{
	memset(data, 0, sizeof(*data));
	
	/* Global defaults */
	data->global.default_patch = 0;  /* Default to patch 0 */
	data->global.midi_channel = 0;  /* Channel 1 (0-indexed) */
	data->global.max_guitars = 4;
	data->global.scan_interval_ms = 100;
	data->global.led_brightness = 128;  /* 50% brightness */
	for (int i = 0; i < 6; i++) {
		data->global.accel_scale[i] = 1000;  /* 1.0x scale (fixed point) */
	}
	data->global.running_average_enable = 1;  /* Enabled */
	data->global.running_average_depth = 5;   /* 5 samples */
	
	/* Initialize all patches with defaults */
	for (int p = 0; p < 127; p++) {
		data->patches[p].velocity_curve = 0; /* Linear */
		data->patches[p].cc_mapping[0] = 16;  /* CC16: General Purpose 1 (X-axis) */
		data->patches[p].cc_mapping[1] = 17;  /* CC17: General Purpose 2 (Y-axis) */
		data->patches[p].cc_mapping[2] = 18;  /* CC18: General Purpose 3 (Z-axis) */
		data->patches[p].cc_mapping[3] = 19;  /* CC19: General Purpose 4 (Roll) */
		data->patches[p].cc_mapping[4] = 20;  /* CC20: General Purpose 5 (Pitch) */
		data->patches[p].cc_mapping[5] = 21;  /* CC21: General Purpose 6 (Yaw) */
		data->patches[p].led_mode = 0;          /* Normal mode */
		data->patches[p].accel_deadzone = 100;  /* 100 units */
		/* Per-axis min/max/invert defaults */
		for (int i = 0; i < 6; i++) {
			data->patches[p].accel_min[i] = 0;    /* Minimum CC value */
			data->patches[p].accel_max[i] = 127;  /* Maximum CC value */
		}
		data->patches[p].accel_invert = 0;  /* No inversion */
		snprintf(data->patches[p].patch_name, sizeof(data->patches[p].patch_name), 
		         "Patch %d", p);
	}
}

int config_storage_init(void)
{
	if (initialized) {
		return 0;
	}
	
	/* Get flash device */
	flash_dev = FLASH_DEVICE;
	if (!device_is_ready(flash_dev)) {
		LOG_ERR("Internal flash device not ready");
		return -ENODEV;
	}
	
	LOG_INF("Internal flash device ready");
	
	/* Check flash parameters */
	const struct flash_parameters *params = flash_get_parameters(flash_dev);
	LOG_INF("Flash write block size: %zu bytes", params->write_block_size);
	LOG_INF("Flash erase value: 0x%02x", params->erase_value);
	
	size_t page_count = flash_get_page_count(flash_dev);
	LOG_INF("Flash page count: %zu", page_count);
	
	/* Verify our structures are aligned */
	LOG_INF("Header size: %zu bytes", sizeof(struct config_header));
	LOG_INF("Data size: %zu bytes", sizeof(struct config_data));
	
	/* Log storage partition info */
	LOG_INF("Storage: offset=0x%08x size=0x%x", CONFIG_FLASH_OFFSET, CONFIG_STORAGE_SIZE);
	LOG_INF("A: 0x%08x (%dKB), B: 0x%08x (%dKB)",
		CONFIG_AREA_A_OFFSET, CONFIG_AREA_A_SIZE / 1024,
		CONFIG_AREA_B_OFFSET, CONFIG_AREA_B_SIZE / 1024);
	
	/* Try to read from both active areas */
	static struct config_header header_a, header_b;
	static struct config_data data_a, data_b;
	
	int ret_a = read_area(CONFIG_AREA_A, &header_a, &data_a);
	int ret_b = read_area(CONFIG_AREA_B, &header_b, &data_b);
	
	/* Determine which area to use based on sequence numbers */
	if (ret_a == 0 && ret_b == 0) {
		/* Both valid - use the one with higher sequence */
		if (header_a.sequence > header_b.sequence) {
			active_area = CONFIG_AREA_A;
			current_sequence = header_a.sequence;
			memcpy(&current_config, &data_a, sizeof(current_config));
			LOG_INF("Using area A (seq=%u)", current_sequence);
		} else {
			active_area = CONFIG_AREA_B;
			current_sequence = header_b.sequence;
			memcpy(&current_config, &data_b, sizeof(current_config));
			LOG_INF("Using area B (seq=%u)", current_sequence);
		}
	} else if (ret_a == 0) {
		/* Only A is valid */
		active_area = CONFIG_AREA_A;
		current_sequence = header_a.sequence;
		memcpy(&current_config, &data_a, sizeof(current_config));
		LOG_INF("Using area A (seq=%u, B invalid)", current_sequence);
	} else if (ret_b == 0) {
		/* Only B is valid */
		active_area = CONFIG_AREA_B;
		current_sequence = header_b.sequence;
		memcpy(&current_config, &data_b, sizeof(current_config));
		LOG_INF("Using area B (seq=%u, A invalid)", current_sequence);
	} else {
		/* Neither valid - use hardcoded defaults */
		LOG_WRN("No valid config found, using hardcoded defaults");
		config_storage_get_hardcoded_defaults(&current_config);
		current_sequence = 0;
		active_area = CONFIG_AREA_A;
		
		/* Try to save hardcoded defaults to active area (non-fatal if it fails) */
		int ret = write_area(CONFIG_AREA_A, &current_config, 1);
		if (ret == 0) {
			current_sequence = 1;
			LOG_INF("Hardcoded defaults written to area A");
		} else {
			LOG_WRN("Failed to write defaults to area A: %d (continuing anyway)", ret);
		}
	}
	
	initialized = true;
	LOG_INF("Configuration storage initialized");
	return 0;
}

int config_storage_save(const struct config_data *data)
{
	if (!initialized) {
		LOG_ERR("Save failed: not initialized");
		return -EACCES;
	}
	
	LOG_DBG("Save: current area=%d, seq=%u", active_area, current_sequence);
	
	/* Determine next area (ping-pong) */
	enum config_area next_area = (active_area == CONFIG_AREA_A) ?
				     CONFIG_AREA_B : CONFIG_AREA_A;
	
	LOG_INF("Save: switching from area %d to area %d", active_area, next_area);
	
	/* Increment sequence number */
	uint32_t next_sequence = current_sequence + 1;
	
	LOG_DBG("Save: next_sequence=%u", next_sequence);
	
	/* Write to inactive area */
	int ret = write_area(next_area, data, next_sequence);
	if (ret != 0) {
		LOG_ERR("Save failed: write_area returned %d", ret);
		return ret;
	}
	
	/* Update current state */
	memcpy(&current_config, data, sizeof(current_config));
	active_area = next_area;
	current_sequence = next_sequence;
	
	LOG_INF("Configuration saved to area %d (seq=%u)",
		active_area, current_sequence);
	return 0;
}

int config_storage_load(struct config_data *data)
{
	if (!initialized) {
		return -EACCES;
	}
	
	memcpy(data, &current_config, sizeof(*data));
	return 0;
}

int config_storage_restore_defaults(void)
{
	if (!initialized) {
		return -EACCES;
	}
	
	static struct config_data data;
	
	/* Load hardcoded defaults */
	LOG_INF("Restoring hardcoded factory defaults");
	config_storage_get_hardcoded_defaults(&data);
	
	/* Save to active storage */
	return config_storage_save(&data);
}

int config_storage_get_info(enum config_area *area, uint32_t *sequence)
{
	if (!initialized) {
		return -EACCES;
	}
	
	if (area) {
		*area = active_area;
	}
	if (sequence) {
		*sequence = current_sequence;
	}
	
	return 0;
}

int config_storage_erase_all(void)
{
	if (!initialized) {
		LOG_ERR("Cannot erase: not initialized");
		return -EACCES;
	}
	
	LOG_WRN("*** ERASING ALL CONFIGURATION STORAGE ***");
	LOG_WRN("This will erase AREA_A and AREA_B");
	
	/* Erase both areas (need to erase 4 pages per 16KB area) */
	int ret;
	
	LOG_INF("Erasing AREA_A at 0x%08x (16KB)...", CONFIG_AREA_A_OFFSET);
	for (int i = 0; i < 4; i++) {
		ret = flash_erase(flash_dev, CONFIG_AREA_A_OFFSET + (i * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
		if (ret != 0) {
			LOG_ERR("Failed to erase AREA_A page %d: %d", i + 1, ret);
			return ret;
		}
	}
	
	LOG_INF("Erasing AREA_B at 0x%08x (16KB)...", CONFIG_AREA_B_OFFSET);
	for (int i = 0; i < 4; i++) {
		ret = flash_erase(flash_dev, CONFIG_AREA_B_OFFSET + (i * FLASH_PAGE_SIZE), FLASH_PAGE_SIZE);
		if (ret != 0) {
			LOG_ERR("Failed to erase AREA_B page %d: %d", i + 1, ret);
			return ret;
		}
	}
	
	LOG_WRN("All configuration areas erased - device will use defaults on next boot");
	LOG_WRN("*** REBOOT REQUIRED ***");
	
	return 0;
}

# Nordic nRF Connect SDK Application Configuration

This document describes the essential configuration requirements for Nordic nRF Connect SDK applications in this workspace, specifically for nRF5340 dual-core targets (nRF5340 Audio DK, Thingy:53).

## Critical Configuration Requirements

### 1. CMakeLists.txt - Project Name

**CRITICAL:** For nRF5340 dual-core applications, the project name MUST be `NONE`.

```cmake
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)  # ← MUST be NONE, not a custom name
```

**Why:** Using `project(NONE)` allows the sysbuild system to properly manage:
- Network core configuration and firmware
- Bootloader integration (MCUboot, b0n)
- Partition management
- Multi-image builds

**Problem:** Using a custom project name (e.g., `project(my_app)`) causes the build system to create the application but it will not function correctly - it may compile and flash but Bluetooth and network core features will not work.

**Fixed in:**
- `client/CMakeLists.txt` - Changed from `project(guitaracc_client)` to `project(NONE)`
- `peripheral_lbs/CMakeLists.txt` - Changed from `project(peripheral_lbs)` to `project(NONE)`

### 2. Kconfig.sysbuild - Sysbuild Configuration

**REQUIRED:** Every nRF5340 application needs a `Kconfig.sysbuild` file at the project root.

**File:** `Kconfig.sysbuild`
```kconfig
#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

source "share/sysbuild/Kconfig"

config NRF_DEFAULT_IPC_RADIO
	default y

config NETCORE_IPC_RADIO_BT_HCI_IPC
	default y
```

**Purpose:**
- Configures the network core (CPUNET) to run the IPC radio firmware
- Enables Bluetooth HCI communication between application core and network core
- Required for all BLE applications on nRF5340

**Added to:**
- `client/Kconfig.sysbuild` - Created
- `peripheral_lbs/Kconfig.sysbuild` - Created

### 3. Kconfig - Application Configuration

**IMPORTANT:** The `Kconfig` file defines configuration options, NOT configuration values.

**Correct Format:**
```kconfig
# Copyright header
# SPDX-License-Identifier: ...

source "Kconfig.zephyr"

# Optional: Add custom menus and config options here
menu "My Application Settings"

config MY_CUSTOM_OPTION
	bool "Enable custom feature"
	default y
	help
	  This is a custom configuration option.

endmenu
```

**WRONG - Do NOT put CONFIG_* assignments in Kconfig:**
```kconfig
# ❌ INCORRECT - These belong in prj.conf
CONFIG_BT_HIDS_MAX_CLIENT_COUNT=2
CONFIG_BT_HIDS_ATTR_MAX=30
```

**Fixed in:**
- `client/Kconfig` - Removed incorrect CONFIG_* assignments

### 4. prj.conf - Project Configuration

Configuration values go in `prj.conf`:

```properties
# Bluetooth
CONFIG_BT=y
CONFIG_BT_PERIPHERAL=y
CONFIG_BT_DEVICE_NAME="My Device"

# DK Library (for LED/button support)
CONFIG_DK_LIBRARY=y
```

## Common Configurations for Workspace Applications

### LED and Button Support

To use Nordic DK LEDs and buttons:

**prj.conf:**
```properties
CONFIG_DK_LIBRARY=y
```

**main.c:**
```c
#include <dk_buttons_and_leds.h>

#define RUN_STATUS_LED  DK_LED1
#define CON_STATUS_LED  DK_LED2

int main(void) {
    int err = dk_leds_init();
    if (err) {
        printk("LEDs init failed (err %d)\n", err);
        return 0;
    }
    
    // Control LEDs
    dk_set_led_on(CON_STATUS_LED);
    dk_set_led_off(CON_STATUS_LED);
}
```

### Connection State Indication

**Pattern:** Show connection state with LED

```c
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err 0x%02x)", err);
    } else {
        LOG_INF("Connected");
        dk_set_led_on(CON_STATUS_LED);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason 0x%02x)", reason);
    dk_set_led_off(CON_STATUS_LED);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};
```

**Implemented in:**
- `client/src/main.c` - Added LED connection indication
- `peripheral_lbs/src/main.c` - Already present

## Board-Specific Configuration

### Thingy:53 Configuration

**File:** `boards/thingy53_nrf5340_cpuapp.conf`

```properties
#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#
# Board-specific overrides for Thingy:53

# Example configurations that may be needed:
# CONFIG_BOOTLOADER_MCUBOOT=y
# CONFIG_NCS_SAMPLE_MCUMGR_BT_OTA_DFU=y
```

This file is automatically included when building for `thingy53_nrf5340_cpuapp` target.

## Build System Integration

### Sysbuild Multi-Image Build

The nRF5340 applications use sysbuild to manage multiple images:

1. **Application** (`client` or `peripheral_lbs`) - Runs on CPUAPP
2. **Network Core** (`ipc_radio` or similar) - Runs on CPUNET, provides BLE HCI
3. **Bootloader** (`mcuboot` or `b0n`) - Optional, for DFU support

**Build Command:**
```bash
west build -b thingy53_nrf5340_cpuapp
```

Sysbuild automatically builds all required images based on:
- `Kconfig.sysbuild` - Sysbuild configuration
- `prj.conf` - Application configuration
- `boards/*.conf` - Board-specific overrides

### Clean Rebuild After Configuration Changes

**IMPORTANT:** After changing `CMakeLists.txt`, `Kconfig.sysbuild`, or `Kconfig`, perform a clean rebuild:

```bash
rm -rf build
west build -b thingy53_nrf5340_cpuapp
```

Incremental builds may not pick up these structural changes.

## Troubleshooting

### Application Builds But Doesn't Work

**Symptom:** Application compiles and flashes successfully, but doesn't advertise or function as expected.

**Likely Causes:**
1. ❌ `project()` name is not `NONE` in CMakeLists.txt
2. ❌ Missing `Kconfig.sysbuild` file
3. ❌ Stale build artifacts - clean rebuild needed

**Solution:** Verify configurations above and perform clean rebuild.

### "Nordic_LBS" Shows in Scanner Instead of Custom Name

**Symptom:** BLE device advertises as "Nordic_LBS" despite CONFIG_BT_DEVICE_NAME setting.

**Cause:** Stale build artifacts from previous configuration or example code.

**Solution:**
1. Verify `CONFIG_BT_DEVICE_NAME` in `prj.conf`
2. Check build config: `grep CONFIG_BT_DEVICE_NAME build/client/zephyr/.config`
3. Clean rebuild: `rm -rf build && west build`

## Summary Checklist for New Applications

- [ ] `CMakeLists.txt` has `project(NONE)`
- [ ] `Kconfig.sysbuild` file exists with IPC radio config
- [ ] `Kconfig` only has config definitions (no CONFIG_* assignments)
- [ ] `prj.conf` has all CONFIG_* settings
- [ ] If using LEDs/buttons: `CONFIG_DK_LIBRARY=y` in prj.conf
- [ ] If board-specific config needed: create `boards/<board>.conf`
- [ ] Clean build after structural changes

## References

- Nordic nRF Connect SDK Documentation: https://developer.nordicsemi.com/
- Zephyr Project Documentation: https://docs.zephyrproject.org/
- This workspace: See `basestation/` and `client/` as working examples

## Change History

- 2026-02-03: Initial documentation based on peripheral_lbs and client fixes
  - Fixed project(NONE) requirement
  - Added Kconfig.sysbuild files
  - Cleaned up Kconfig vs prj.conf distinction
  - Added LED connection state indication to client

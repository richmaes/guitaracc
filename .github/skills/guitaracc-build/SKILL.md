---
name: guitaracc-build
description: 'Build GuitarAcc basestation or client firmware with automatic environment setup. Use when: building firmware, compiling basestation, compiling client, fixing build errors, running west build, setting up nRF SDK environment, activating build environment.'
argument-hint: 'target (basestation/client) and options (clean/pristine)'
---

# GuitarAcc Firmware Build

Automated build workflow for GuitarAcc basestation and client firmware with automatic nRF Connect SDK environment setup.

> **⚡ Key Approach:** Always use `nrfutil toolchain-manager launch --ncs-version v3.2.1 -- <command>` to automatically configure the complete build environment. This avoids manual environment sourcing and prevents freezing/timeout issues.

## When to Use

- Building basestation or client firmware
- Troubleshooting build errors
- Running clean/pristine builds
- Setting up the build environment
- Verifying build configuration
- After making code changes

## GuitarAcc Build Environment

### Project Structure
```
guitaracc_test/
├── venv/                    # Python virtual environment (activate before building)
├── basestation/             # nRF5340 Audio DK firmware
│   ├── src/
│   ├── CMakeLists.txt
│   ├── prj.conf
│   └── build/               # Build output
└── client/                  # Thingy:53 firmware
    ├── src/
    ├── CMakeLists.txt
    ├── prj.conf
    └── build/               # Build output
```

### Required Tools
- **nRF Connect SDK v3.2.1** - Nordic's Zephyr-based SDK
- **west** - Zephyr build tool (provided by nRF SDK)
- **Python virtual environment** - Project-specific Python packages
- **Toolchain** - ARM GCC compiler (provided by nRF SDK)

### Board Targets
- **Basestation**: `nrf5340_audio_dk/nrf5340/cpuapp`
- **Client**: `thingy53/nrf5340/cpuapp`

## Build Workflow

**TL;DR:** Use `nrfutil toolchain-manager launch` to automatically set up the complete build environment.

### Step 1: Activate Python Virtual Environment

The project includes a Python venv that must be activated:

```bash
cd /Users/richmaes/src/guitaracc_test
source venv/bin/activate
```

**Verification:**
```bash
which python  # Should show venv/bin/python
```

### Step 2: Build Using nrfutil toolchain-manager

**The correct way to build is using `nrfutil toolchain-manager`** which automatically sets up the complete toolchain environment, avoiding the need to manually source environment files or manage paths.

**Why this works:**
- Automatically configures ZEPHYR_TOOLCHAIN_VARIANT, ZEPHYR_SDK_INSTALL_DIR, and PATH
- Ensures ARM GCC compiler is available
- Avoids environment variable conflicts
- Works reliably without freezing or timeout issues

### Step 3: Build the Firmware

**Incremental Build (fast, for code changes):**
```bash
cd /Users/richmaes/src/guitaracc_test
source venv/bin/activate
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west build -d basestation/build -b nrf5340_audio_dk/nrf5340/cpuapp basestation
```

**Pristine Build (clean, for config changes):**
```bash
cd /Users/richmaes/src/guitaracc_test
source venv/bin/activate
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west build -d basestation/build -b nrf5340_audio_dk/nrf5340/cpuapp basestation --pristine
```

**Using the Build Script:**
```bash
cd /Users/richmaes/src/guitaracc_test
.github/skills/guitaracc-build/scripts/build.sh basestation          # Incremental
.github/skills/guitaracc-build/scripts/build.sh basestation pristine # Clean
```

**Note:** Use generous timeouts (180 seconds) when building to allow complete compilation.

### Step 4: Verify Build Success

**Check for build artifacts:**
```bash
ls -lh basestation/build/basestation/zephyr/zephyr.{elf,hex,bin}
```

**Note:** Build artifacts are in `basestation/build/basestation/zephyr/`, not `basestation/build/zephyr/` due to sysbuild structure.

**Check for errors:**
```bash
grep -i "error:" basestation/build/CMakeFiles/*.log
tail -50 basestation/build/build.log  # If using Makefile
```

### Step 5: Flash to Device (Optional)

```bash
cd /Users/richmaes/src/guitaracc_test
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west flash -d basestation/build
```

## Complete Build Script

A complete build script is available at `.github/skills/guitaracc-build/scripts/build.sh`:

**Usage:**
```bash
cd /Users/richmaes/src/guitaracc_test

# Incremental build
.github/skills/guitaracc-build/scripts/build.sh basestation

# Pristine build
.github/skills/guitaracc-build/scripts/build.sh basestation pristine

# Client build
.github/skills/guitaracc-build/scripts/build.sh client pristine
```

**What it does:**
1. Activates Python virtual environment
2. Uses nrfutil toolchain-manager to set up complete toolchain
3. Builds with west for the specified target
4. Verifies build artifacts exist
5. Shows paths for flashing

## Common Build Issues

### Issue: "west: command not found"

**Cause:** Not in nRF SDK environment

**Solution:**
Use `nrfutil toolchain-manager launch` instead of trying to source environment manually:
```bash
cd /Users/richmaes/src/guitaracc_test
source venv/bin/activate
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west build -d basestation/build -b nrf5340_audio_dk/nrf5340/cpuapp basestation --pristine
```

### Issue: "Zephyr was unable to find the toolchain" or "ZEPHYR_TOOLCHAIN_VARIANT: gnuarmemb"

**Cause:** Environment not properly configured, wrong toolchain variant

**Solution:**
Always use `nrfutil toolchain-manager launch` which automatically configures:
- ZEPHYR_TOOLCHAIN_VARIANT=zephyr
- ZEPHYR_SDK_INSTALL_DIR 
- PATH with ARM compiler
```bash
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- <command>
```

### Issue: Build freezes or times out

**Cause:** Environment sourcing can hang, especially with interactive shells

**Solution:**
- Use `nrfutil toolchain-manager launch` with explicit commands (not --shell)
- Set generous timeouts (180 seconds for full builds)
- Avoid manual environment variable exports

### Issue: "CMake Error: The source directory does not exist"

**Cause:** Running west build from wrong directory

**Solution:**
```bash
cd /Users/richmaes/src/guitaracc_test/basestation  # Must be in app directory
west build -b nrf5340_audio_dk/nrf5340/cpuapp
```

### Issue: "Python interpreter not found" or "Module not found"

**Cause:** Virtual environment not activated or Python path issues

**Solution:**
```bash
cd /Users/richmaes/src/guitaracc_test
source venv/bin/activate
# Verify:
which python  # Should show venv/bin/python
```

### Issue: "Board nrf5340_audio_dk/nrf5340/cpuapp not found"

**Cause:** Wrong nRF SDK version or board name

**Solution:**
- Verify SDK version: `west --version`
- Check board name spelling (use underscores, not dashes)
- Correct: `nrf5340_audio_dk/nrf5340/cpuapp`

### Issue: Build succeeds but old firmware version shows

**Cause:** Firmware flashed successfully but device not reset, or build artifacts not updated

**Solution:**
```bash
# Pristine build
west build -b nrf5340_audio_dk/nrf5340/cpuapp --pristine

# Flash and verify
west flash

# Check version on device
# Connect to serial terminal and run: status
```

### Issue: "implicit declaration of function" errors

**Cause:** Missing include files or configuration

**Solution:**
```bash
# Clean build to regenerate all configs
west build -b nrf5340_audio_dk/nrf5340/cpuapp --pristine
```

### Issue: Float printf shows "*float*" instead of values

**Cause:** Missing `CONFIG_CBPRINTF_FP_SUPPORT`

**Solution:**
Add to `prj.conf`:
```
CONFIG_CBPRINTF_FP_SUPPORT=y
```

## Build Verification Checklist

After any build:

- [ ] **Venv activated** - `which python` shows venv path
- [ ] **West available** - `which west` shows a path
- [ ] **Build succeeded** - No errors in output
- [ ] **Artifacts exist** - `ls basestation/build/zephyr/zephyr.hex`
- [ ] **Version updated** - Firmware version matches CMakeLists.txt
- [ ] **No warnings** - Check build log for concerning warnings

Before flashing:

- [ ] **Device connected** - `nrfjprog -i` shows device
- [ ] **Correct target** - Basestation on Audio DK, Client on Thingy:53
- [ ] **Version incremented** - If code changes were made

After flashing:

- [ ] **Device boots** - Connect serial terminal, see output
- [ ] **Version correct** - Run `status` command, verify version
- [ ] **New features work** - Test your changes

## Quick Reference

### Build Commands

```bash
# Activate venv first (always)
cd /Users/richmaes/src/guitaracc_test && source venv/bin/activate

# Incremental build (fastest)
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west build -d basestation/build -b nrf5340_audio_dk/nrf5340/cpuapp basestation

# Pristine build (clean everything)
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west build -d basestation/build -b nrf5340_audio_dk/nrf5340/cpuapp basestation --pristine

# Using build script
.github/skills/guitaracc-build/scripts/build.sh basestation pristine

# Flash
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- \
  west flash -d basestation/build

# Clean build directory
rm -rf basestation/build
```

### Environment Setup

```bash
# Activate venv (required)
cd /Users/richmaes/src/guitaracc_test && source venv/bin/activate

# Check environment (inside nrfutil launch)
which west
echo $ZEPHYR_BASE
west --version
```

### File Locations

- **Source code**: `basestation/src/*.c`
- **CMake config**: `basestation/CMakeLists.txt`
- **Version**: `basestation/CMakeLists.txt` (project() version)
- **Zephyr config**: `basestation/prj.conf`
- **Build output**: `basestation/build/basestation/zephyr/`
- **Firmware files**: 
  - `basestation/build/basestation/zephyr/zephyr.hex`
  - `basestation/build/basestation/zephyr/zephyr.elf`
  - `basestation/build/basestation/zephyr/zephyr.bin`
- **Merged image**: `basestation/build/merged.hex`
- **Build script**: `.github/skills/guitaracc-build/scripts/build.sh`

## Additional Resources

- [Zephyr West Build Documentation](https://docs.zephyrproject.org/latest/develop/west/build-flash-debug.html)
- [nRF Connect SDK Documentation](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/index.html)
- [GuitarAcc VERSIONING.md](../../basestation/VERSIONING.md) - Firmware version management
- [GuitarAcc BUILD_SYSTEM.md](../../basestation/BUILD_SYSTEM.md) - CMake configuration details

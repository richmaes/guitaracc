# GuitarAcc Build Skill

Automated build workflow for GuitarAcc basestation and client firmware with automatic nRF Connect SDK environment setup.

## Quick Start

```bash
# Build basestation (pristine)
.github/skills/guitaracc-build/scripts/build.sh basestation pristine

# Build basestation (incremental)
.github/skills/guitaracc-build/scripts/build.sh basestation

# Build client
.github/skills/guitaracc-build/scripts/build.sh client pristine
```

## Key Learnings

### Always Use nrfutil toolchain-manager

The correct way to build is:
```bash
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- <build-command>
```

**Why:**
- Automatically configures toolchain environment
- Sets ZEPHYR_TOOLCHAIN_VARIANT=zephyr
- Provides ARM GCC compiler in PATH
- Avoids environment variable conflicts
- Prevents freezing/timeout issues

### Don't Manually Source Environment

❌ **Don't do this:**
```bash
source /opt/nordic/ncs/v3.2.1/zephyr/zephyr-env.sh
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
```

✅ **Do this:**
```bash
nrfutil toolchain-manager launch --ncs-version v3.2.1 -- west build ...
```

### Build Artifact Paths

Build outputs are in sysbuild structure:
- ✅ `basestation/build/basestation/zephyr/zephyr.hex`
- ❌ NOT `basestation/build/zephyr/zephyr.hex`

## Files

- **SKILL.md**: Complete build documentation and troubleshooting
- **scripts/build.sh**: Automated build script
- **README.md**: This file

## See Also

- [SKILL.md](SKILL.md) - Full documentation
- [../../ARCHITECTURE.md](../../ARCHITECTURE.md) - System architecture
- [../../basestation/BUILD_SYSTEM.md](../../basestation/BUILD_SYSTEM.md) - CMake details

Automated workflow skill for building GuitarAcc basestation and client firmware with proper nRF Connect SDK environment setup.

## Purpose

This skill provides comprehensive guidance for:
- Setting up the nRF Connect SDK build environment
- Activating the Python virtual environment
- Building basestation and client firmware
- Troubleshooting common build issues
- Verifying build outputs

## Files in This Skill

- **SKILL.md** - Complete build workflow documentation with:
  - Environment setup instructions
  - Step-by-step build process
  - Common issues and solutions
  - Build verification checklist
  - Quick reference commands

- **scripts/build.sh** - Automated build script that:
  - Activates Python venv
  - Checks for west availability
  - Builds firmware with proper board targets
  - Verifies build artifacts
  - Reports firmware version

## Usage

### Using the Skill in Copilot

When you ask Copilot to build firmware, it will automatically:
1. Detect that you're working with GuitarAcc
2. Load this skill based on the keywords (build, compile, west, firmware)
3. Follow the documented workflow
4. Handle environment setup automatically

Example prompts that trigger this skill:
- "Build the basestation"
- "Compile basestation firmware"
- "Run a clean build"
- "I'm getting 'west: command not found'"

### Using the Build Script Directly

```bash
# Navigate to project root
cd /Users/richmaes/src/guitaracc_test

# Run the build script (must be in nRF Connect terminal)
.github/skills/guitaracc-build/scripts/build.sh basestation

# For a clean build
.github/skills/guitaracc-build/scripts/build.sh basestation pristine

# Build client
.github/skills/guitaracc-build/scripts/build.sh client
```

**Important:** The build script must be run in a terminal where `west` is available:
- Use nRF Connect extension: Cmd+Shift+P → "nRF Connect: Create Shell Terminal"
- Or activate SDK manually: `nrfutil sdk-manager toolchain launch --ncs-version v3.2.1 --shell`

## Prerequisites

- nRF Connect SDK v3.2.1 installed
- nRF Connect for VS Code extension
- Python virtual environment at `/Users/richmaes/src/guitaracc_test/venv/`
- west tool (provided by nRF SDK)

## Build Targets

- **Basestation**: nRF5340 Audio DK (`nrf5340_audio_dk/nrf5340/cpuapp`)
- **Client**: Thingy:53 (`thingy53/nrf5340/cpuapp`)

## Quick Reference

### Environment Setup
```bash
# Activate venv
cd /Users/richmaes/src/guitaracc_test
source venv/bin/activate

# Check for west
which west

# Use nRF Connect terminal (easiest)
# VS Code: Cmd+Shift+P → "nRF Connect: Create Shell Terminal"
```

### Build Commands
```bash
# Incremental build
cd basestation
west build -b nrf5340_audio_dk/nrf5340/cpuapp

# Clean build
cd basestation
west build -b nrf5340_audio_dk/nrf5340/cpuapp --pristine

# Flash
west flash
```

### Using Makefile
```bash
# Build with tests
make build-basestation

# Flash
make flash-basestation
```

## Related Documentation

- [basestation/BUILD_SYSTEM.md](../../../basestation/BUILD_SYSTEM.md) - CMake configuration
- [basestation/VERSIONING.md](../../../basestation/VERSIONING.md) - Version management
- [guitaracc-cli-commands skill](../guitaracc-cli-commands/) - For adding/modifying CLI commands

## Skill Metadata

- **Skill Type**: Workflow/Build Automation
- **Keywords**: build, compile, west, firmware, nrf, basestation, client, environment
- **Dependencies**: nRF Connect SDK, west, Python venv
- **Platform**: Nordic nRF5340, Zephyr RTOS

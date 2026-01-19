# GuitarAcc Project - Top-Level Build & Test
#
# This Makefile orchestrates the complete build and test process:
# 1. Run host-based unit tests for all applications
# 2. If tests pass, build firmware for target hardware
# 3. Optionally flash to connected devices

.PHONY: all test build flash clean help check-west init-west
.PHONY: test-basestation test-client test-integration
.PHONY: build-basestation build-client
.PHONY: flash-basestation flash-client

# Default target: test then build
all: test build

# Nordic nRF Connect SDK configuration
NCS_BASE := /opt/nordic/ncs
NCS_VERSION := v3.2.1

WEST_PYTHON := /opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12

# Colors for output
GREEN := \033[0;32m
YELLOW := \033[0;33m
RED := \033[0;31m
NC := \033[0m # No Color

init-west:
	@nrfutil sdk-manager toolchain launch --ncs-version $(NCS_VERSION) --shell
	@source $(NCS_BASE)/$(NCS_VERSION)/zephyr/zephyr-env.sh

# Check if west is available
check-west:
	@which west > /dev/null 2>&1 || \
	(echo "$(RED)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)" && \
	 echo "$(RED)ERROR: 'west' command not found$(NC)" && \
	 echo "$(RED)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)" && \
	 echo "" && \
	 echo "$(YELLOW)The 'west' build tool is required for firmware builds.$(NC)" && \
	 echo "" && \
	 echo "$(YELLOW)To fix this:$(NC)" && \
	 echo "  1. In VS Code, open the Command Palette (Cmd+Shift+P)" && \
	 echo "  2. Run: 'nRF Connect: Create Shell Terminal'" && \
	 echo "  3. In the nRF Connect SDK terminal, run: make <target>" && \
	 echo "" && \
	 echo "$(YELLOW)Or from the current terminal:$(NC)" && \
	 echo "  1. Tests only (no west needed): make test" && \
	 echo "  2. Tests only (no west needed): make verify" && \
	 echo "" && \
	 echo "$(YELLOW)Or manually initialize nRF Connect SDK:$(NC)" && \
	 echo "  nrfutil sdk-manager toolchain launch --ncs-version $(NCS_VERSION) --shell" && \
	 echo "  source $(NCS_BASE)/$(NCS_VERSION)/zephyr/zephyr-env.sh" && \
	 echo "" && \
	 echo "$(YELLOW)Current SDK: nRF Connect SDK $(NCS_VERSION)$(NC)" && \
	 echo "$(YELLOW)Working directory: $(shell pwd)$(NC)" && \
	 echo "" && \
	 exit 1)

#==============================================================================
# Testing Targets
#==============================================================================

test: test-basestation test-client test-integration
	@echo ""
	@echo "$(GREEN)✅ All tests passed!$(NC)"
	@echo ""

test-basestation:
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Testing Basestation Application$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@cd basestation/test && $(MAKE) test
	@echo "$(GREEN)✓ Basestation tests passed$(NC)"

test-client:
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Testing Client Application$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@cd client/test && $(MAKE) test
	@echo "$(GREEN)✓ Client tests passed$(NC)"

test-integration:
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Integration Tests (Software-in-the-Loop)$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@cd integration_test && $(MAKE) run
	@echo "$(GREEN)✓ Integration tests passed$(NC)"

#==============================================================================
# Build Targets
#==============================================================================

build: test check-west build-basestation build-client
	@echo ""
	@echo "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(GREEN)✅ All applications built successfully!$(NC)"
	@echo "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""
	@echo "Build artifacts:"
	@echo "  Basestation: basestation/build/zephyr/zephyr.hex"
	@echo "  Client:      client/build/zephyr/merged.hex (DFU package)"
	@echo ""

build-basestation: check-west
	@echo ""
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Building Basestation for nRF5340 Audio DK$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@cd basestation && west build -b nrf5340_audio_dk/nrf5340/cpuapp --build-dir build
	@echo "$(GREEN)✓ Basestation build complete$(NC)"

build-client: check-west
	@echo ""
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Building Client for Thingy:53$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@cd client && CMAKE=/opt/nordic/ncs/toolchains/322ac893fe/Cellar/cmake/3.21.0/bin/cmake west build -b thingy53/nrf5340/cpuapp --build-dir build --sysbuild -- -DPython3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Dclient_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Dmcuboot_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Dempty_net_core_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Db0n_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12
	@echo "$(GREEN)✓ Client build complete$(NC)"

#==============================================================================
# Rebuild Targets (pristine builds)
#==============================================================================

rebuild: test check-west rebuild-basestation rebuild-client
	@echo ""
	@echo "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(GREEN)✅ Pristine rebuild complete!$(NC)"
	@echo "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo ""

rebuild-basestation: check-west
	@echo ""
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Pristine build: Basestation$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@cd basestation && west build -b nrf5340_audio_dk/nrf5340/cpuapp --build-dir build --pristine
	@echo "$(GREEN)✓ Basestation pristine build complete$(NC)"

rebuild-client: check-west
	@echo ""
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Pristine build: Client$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
#	@cd client && CMAKE=/opt/nordic/ncs/toolchains/322ac893fe/Cellar/cmake/3.21.0/bin/cmake west build -b thingy53/nrf5340/cpuapp --build-dir build --sysbuild --pristine -- -DPython3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Dclient_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Dmcuboot_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Dempty_net_core_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12 -Db0n_Python3_EXECUTABLE=/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12
	@cd client && CMAKE_COMMAND=/opt/nordic/ncs/toolchains/322ac893fe/Cellar/cmake/3.21.0/bin/cmake && west build --build-dir /Users/richmaes/src/guitaracc/client/build /Users/richmaes/src/guitaracc/client --pristine --board thingy53/nrf5340/cpuapp --sysbuild -- -DDEBUG_THREAD_INFO=Off -DPython3_EXECUTABLE=$(WEST_PYTHON) -Dclient_Python3_EXECUTABLE=$($(WEST_PYTHON)) -Dmcuboot_Python3_EXECUTABLE=$(WEST_PYTHON) -Dempty_net_core_Python3_EXECUTABLE=$(WEST_PYTHON) -Db0n_Python3_EXECUTABLE=$(WEST_PYTHON) -Dclient_DEBUG_THREAD_INFO=Off -Dmcuboot_DEBUG_THREAD_INFO=Off -Dempty_net_core_DEBUG_THREAD_INFO=Off -Db0n_DEBUG_THREAD_INFO=Off
	@echo "$(GREEN)✓ Client pristine build complete$(NC)"

#==============================================================================
# Flash Targets
#==============================================================================

flash: check-west flash-basestation flash-client
	@echo ""
	@echo "$(GREEN)✅ All devices flashed!$(NC)"
	@echo ""

flash-basestation: check-west
	@echo ""
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Flashing Basestation (nRF5340 Audio DK)$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)⚠ Connect nRF5340 Audio DK and press Enter...$(NC)"
	@read dummy
	@cd basestation && west flash --build-dir build
	@echo "$(GREEN)✓ Basestation flashed$(NC)"

flash-client: check-west
	@echo ""
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)Flashing Client (Thingy:53)$(NC)"
	@echo "$(YELLOW)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(YELLOW)⚠ Connect Thingy:53 via USB and press Enter...$(NC)"
	@read dummy
	@cd client && west flash --build-dir build
	@echo "$(GREEN)✓ Client flashed$(NC)"

#==============================================================================
# Clean Targets
#==============================================================================

	@cd integration_test && $(MAKE) clean || true
clean: clean-tests clean-builds
	@echo "$(GREEN)✓ Cleanup complete$(NC)"

clean-tests:
	@echo "Cleaning test artifacts..."
	@cd basestation/test && $(MAKE) clean || true
	@cd client/test && $(MAKE) clean || true

clean-builds:
	@echo "Cleaning build artifacts..."
	@rm -rf basestation/build
	@rm -rf client/build
	@echo "$(GREEN)✓ Build directories removed$(NC)"

#==============================================================================
# Development Workflow Targets
#==============================================================================

# Quick test-and-build for rapid iteration
quick: test build-basestation
	@echo "$(GREEN)✓ Quick build complete (basestation only)$(NC)"

# Verify code without building firmware
verify: test
	@echo "$(GREEN)✓ Verification complete - code is ready to build$(NC)"

# Monitor serial output from basestation
monitor-basestation:
	@echo "$(YELLOW)Monitoring basestation serial output...$(NC)"
	@echo "$(YELLOW)Press Ctrl-A then K to exit$(NC)"
	@sleep 1
	@DEVICE=$$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1); \
	if [ -z "$$DEVICE" ]; then \
		DEVICE=$$(ls /dev/tty.usbmodem* 2>/dev/null | head -n 1); \
	fi; \
	if [ -z "$$DEVICE" ]; then \
		echo "$(RED)Error: No USB modem device found$(NC)"; \
		exit 1; \
	fi; \
	echo "Found device: $$DEVICE"; \
	screen $$DEVICE 115200

# Monitor serial output from client
monitor-client:
	@echo "$(YELLOW)Monitoring client serial output...$(NC)"
	@echo "$(YELLOW)Press Ctrl-A then K to exit$(NC)"
	@sleep 1
	@DEVICE=$$(ls /dev/cu.usbmodem* 2>/dev/null | tail -n 1); \
	if [ -z "$$DEVICE" ]; then \
		DEVICE=$$(ls /dev/tty.usbmodem* 2>/dev/null | tail -n 1); \
	fi; \
	if [ -z "$$DEVICE" ]; then \
		echo "$(RED)Error: No USB modem device found$(NC)"; \
		exit 1; \
	fi; \
	echo "Found device: $$DEVICE"; \
	screen $$DEVICE 115200

# CI/CD Targets
#==============================================================================

# Run all tests and build all targets (for CI)
ci: clean check-west test build
	@echo "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"
	@echo "$(GREEN)✅ CI build successful!$(NC)"
	@echo "$(GREEN)━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━$(NC)"

#==============================================================================
# Help
#==============================================================================

help:
	@echo "GuitarAcc Project - Build & Test System"
	@echo ""
	@echo "$(YELLOW)Primary Targets:$(NC)"
	@echo "  make all              - Run tests, then buitests (unit + integration)"
	@echo "  make build            - Build firmware for all targets (after tests)"
	@echo "  make rebuild          - Clean and rebuild everything from scratch"
	@echo "  make flash            - Flash firmware to all connected devices"
	@echo ""
	@echo "$(YELLOW)Individual Application Targets:$(NC)"
	@echo "  make test-basestation        - Test basestation logic"
	@echo "  make test-client             - Test client motion detection logic"
	@echo "  make test-integration        - Software-in-the-Loop integration tests
	@echo "  make test-client             - Test client motion detection logic"
	@echo "  make build-basestation       - Build basestation firmware"
	@echo "  make build-client            - Build client firmware"
	@echo "  make rebuild-basestation     - Pristine rebuild of basestation"
	@echo "  make rebuild-client          - Pristine rebuild of client"
	@echo "  make flash-basestation       - Flash basestation to nRF5340 Audio DK"
	@echo "  make flash-client            - Flash client to Thingy:53"
	@echo ""
	@echo "$(YELLOW)Development Workflow:$(NC)"
	@echo "  make quick            - Fast test + build (basestation only)"
	@echo "  make verify           - Run tests only (no firmware build)"
	@echo "  make monitor-basestation    - Connect to basestation serial output"
	@echo "  make monitor-client         - Connect to client serial output"
	@echo ""
	@echo "$(YELLOW)Maintenance:$(NC)"
	@echo "  make clean            - Remove all build artifacts and test binaries"
	@echo "  make clean-tests      - Remove only test artifacts"
	@echo "  make clean-builds     - Remove only firmware build directories"
	@echo ""
	@echo "$(YELLOW)CI/CD:$(NC)"
	@echo "  make ci               - Full clean build for continuous integration"
	@echo ""
	@echo "$(YELLOW)Build Artifacts:$(NC)"
	@echo "  Basestation: basestation/build/zephyr/zephyr.hex"
	@echo "  Client:      client/build/zephyr/merged.hex"
	@echo ""
	@echo "$(YELLOW)Typical Workflows:$(NC)"
	@echo "  Development:   make quick && make flash-basestation"
	@echo "  Full rebuild:  make rebuild && make flash"
	@echo "  Verification:  make verify"
	@echo "  CI pipeline:   make ci"
	@echo ""

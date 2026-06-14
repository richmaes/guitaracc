# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# ============================================================================
# Firmware Version Configuration
# ============================================================================
# Update FIRMWARE_VERSION_MINOR for each build/change to track versions
set(FIRMWARE_VERSION_MAJOR 1)
set(FIRMWARE_VERSION_MINOR 3)
set(FIRMWARE_VERSION_PATCH 1)
set(FIRMWARE_VERSION "${FIRMWARE_VERSION_MAJOR}.${FIRMWARE_VERSION_MINOR}.${FIRMWARE_VERSION_PATCH}")

# Force use of nRF SDK Python - check multiple sources
if(DEFINED WEST_PYTHON)
  set(Python3_EXECUTABLE "${WEST_PYTHON}" CACHE FILEPATH "Python executable" FORCE)
elseif(DEFINED ENV{WEST_PYTHON})
  set(Python3_EXECUTABLE "$ENV{WEST_PYTHON}" CACHE FILEPATH "Python executable" FORCE)
elseif(EXISTS "/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12")
  # Fallback to known SDK Python location
  set(Python3_EXECUTABLE "/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12" CACHE FILEPATH "Python executable" FORCE)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(guitaracc_basestation)

# ============================================================================
# Generate version header file
# ============================================================================
set(VERSION_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(VERSION_HEADER "${VERSION_HEADER_DIR}/firmware_version.h")

# Create include directory
file(MAKE_DIRECTORY "${VERSION_HEADER_DIR}")

# Generate version header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/firmware_version.h.in"
    "${VERSION_HEADER}"
    @ONLY
)

# Add generated include directory to build
target_include_directories(app PRIVATE "${VERSION_HEADER_DIR}")

# Display version during build
message(STATUS "GuitarAcc Basestation Firmware Version: ${FIRMWARE_VERSION}")

# ============================================================================
# Source files
# ============================================================================
target_sources(app PRIVATE 
    src/main.c
    src/midi_logic.c
    src/accel_mapping.c
    src/ui_led.c
    src/ui_interface_shell.c
    src/config_storage.c
)

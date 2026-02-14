# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

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

target_sources(app PRIVATE 
    src/main.c
    src/midi_logic.c
    src/accel_mapping.c
)

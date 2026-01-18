# Sysbuild configuration hook
# Explicitly pass Python executable to each child image

message(STATUS "[SYSBUILD DEBUG] ===== sysbuild.cmake executing =====")
message(STATUS "[SYSBUILD DEBUG] CMAKE_COMMAND: ${CMAKE_COMMAND}")
message(STATUS "[SYSBUILD DEBUG] Python3_EXECUTABLE: ${Python3_EXECUTABLE}")
message(STATUS "[SYSBUILD DEBUG] WEST_PYTHON: ${WEST_PYTHON}")

# For each image that sysbuild creates, if we have a <image>_Python3_EXECUTABLE variable,
# pass it as -DPython3_EXECUTABLE to that image's CMake
if(DEFINED mcuboot_Python3_EXECUTABLE)
  message(STATUS "[SYSBUILD DEBUG] mcuboot_Python3_EXECUTABLE: ${mcuboot_Python3_EXECUTABLE}")
  set(mcuboot_EXTRA_CMAKE_ARGS "${mcuboot_EXTRA_CMAKE_ARGS} -DPython3_EXECUTABLE=${mcuboot_Python3_EXECUTABLE}" CACHE STRING "Extra CMake args for mcuboot" FORCE)
  message(STATUS "[SYSBUILD DEBUG] mcuboot_EXTRA_CMAKE_ARGS: ${mcuboot_EXTRA_CMAKE_ARGS}")
endif()

if(DEFINED client_Python3_EXECUTABLE)
  message(STATUS "[SYSBUILD DEBUG] client_Python3_EXECUTABLE: ${client_Python3_EXECUTABLE}")
  set(client_EXTRA_CMAKE_ARGS "${client_EXTRA_CMAKE_ARGS} -DPython3_EXECUTABLE=${client_Python3_EXECUTABLE}" CACHE STRING "Extra CMake args for client" FORCE)
  message(STATUS "[SYSBUILD DEBUG] client_EXTRA_CMAKE_ARGS: ${client_EXTRA_CMAKE_ARGS}")
endif()

if(DEFINED empty_net_core_Python3_EXECUTABLE)
  message(STATUS "[SYSBUILD DEBUG] empty_net_core_Python3_EXECUTABLE: ${empty_net_core_Python3_EXECUTABLE}")
  set(empty_net_core_EXTRA_CMAKE_ARGS "${empty_net_core_EXTRA_CMAKE_ARGS} -DPython3_EXECUTABLE=${empty_net_core_Python3_EXECUTABLE}" CACHE STRING "Extra CMake args for empty_net_core" FORCE)
endif()

if(DEFINED b0n_Python3_EXECUTABLE)
  message(STATUS "[SYSBUILD DEBUG] b0n_Python3_EXECUTABLE: ${b0n_Python3_EXECUTABLE}")
  set(b0n_EXTRA_CMAKE_ARGS "${b0n_EXTRA_CMAKE_ARGS} -DPython3_EXECUTABLE=${b0n_Python3_EXECUTABLE}" CACHE STRING "Extra CMake args for b0n" FORCE)
endif()

message(STATUS "[SYSBUILD DEBUG] ===== sysbuild.cmake complete =====")

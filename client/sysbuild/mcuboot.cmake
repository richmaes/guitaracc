# Force MCUboot to use SDK Python
# Override before it calls find_package(Zephyr)

message(STATUS "[MCUBOOT DEBUG] WEST_PYTHON value: '${WEST_PYTHON}'")
message(STATUS "[MCUBOOT DEBUG] Python3_EXECUTABLE before: '${Python3_EXECUTABLE}'")

if(DEFINED WEST_PYTHON)
  message(STATUS "[MCUBOOT DEBUG] Using WEST_PYTHON path")
  set(Python3_EXECUTABLE "${WEST_PYTHON}" CACHE FILEPATH "Python executable" FORCE)
elseif(EXISTS "/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12")
  message(STATUS "[MCUBOOT DEBUG] Using fallback SDK Python path")
  set(Python3_EXECUTABLE "/opt/nordic/ncs/toolchains/322ac893fe/opt/python@3.12/bin/python3.12" CACHE FILEPATH "Python executable" FORCE)
else()
  message(STATUS "[MCUBOOT DEBUG] No Python override applied")
endif()

message(STATUS "[MCUBOOT DEBUG] Python3_EXECUTABLE after: '${Python3_EXECUTABLE}'")

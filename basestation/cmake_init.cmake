# Force CMake to use the correct Python interpreter
# This ensures pykwalify and other dependencies are available
if(DEFINED WEST_PYTHON)
  set(Python3_EXECUTABLE "${WEST_PYTHON}" CACHE FILEPATH "Python executable" FORCE)
endif()

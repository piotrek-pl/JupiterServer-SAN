# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\JupiterServer_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\JupiterServer_autogen.dir\\ParseCache.txt"
  "JupiterServer_autogen"
  )
endif()

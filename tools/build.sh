#!/bin/bash
# ==============================================================================
# tools/build.sh
# ==============================================================================

# Set the maximum number of concurrent processes during `cmake --build`
export CMAKE_BUILD_PARALLEL_LEVEL=4

# Run CMake configure stage
cmake --preset debug

# Run CMake build stage
cmake --build --preset debug

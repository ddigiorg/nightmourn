cmake_minimum_required(VERSION 3.25)
project(Nightmourn LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Nezumi
#
# Fetched at configure time, pinned to a release tag. Nezumi in turn fetches
# its own dependencies, and builds only the engine (no examples or tests)
# because it is not the top-level project here.
#
# To develop against a local engine checkout instead:
#   cmake --preset debug -D FETCHCONTENT_SOURCE_DIR_NEZUMI=../nezumi
# ------------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(nezumi
    GIT_REPOSITORY git@github.com:ddigiorg/nezumi.git
    GIT_TAG        dev
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nezumi)

# ------------------------------------------------------------------------------
# Nightmourn
# ------------------------------------------------------------------------------

add_executable(nightmourn src/main.cpp)
target_link_libraries(nightmourn PRIVATE Nezumi::Engine)

# Copy the engine's runtime assets (shaders, fonts) next to the executable,
# followed by this game's own assets/ folder once it has one.
nezumi_copy_assets(nightmourn ENGINE)

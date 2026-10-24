cmake_minimum_required(VERSION 3.25)
project(Nightmourn LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NIGHTMOURN_BUILD_DEMOS "Build the Nightmourn demos" ON)

# ------------------------------------------------------------------------------
# Nezumi
#
# Fetched at configure time, pinned to a commit on Nezumi's dev branch. Nezumi
# in turn fetches its own dependencies, and builds only the engine (no
# examples or tests) because it is not the top-level project here.
#
# To move to a newer engine, change GIT_TAG to the new commit SHA. A branch
# name would also work, but then two configures a day apart can build against
# different engines.
#
# To develop against a local engine checkout instead, use the `dev-local`
# preset from CMakeUserPresets.json (see README.md), or:
#   cmake --preset debug -D FETCHCONTENT_SOURCE_DIR_NEZUMI=../nezumi
# ------------------------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(nezumi
    GIT_REPOSITORY https://github.com/ddigiorg/nezumi.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(nezumi)

# ------------------------------------------------------------------------------
# Game library
#
# Everything the game and its demos share lives in src/Nightmourn/ and is
# included as <Nightmourn/...>, the same way the engine is included as
# <Nezumi/...>. The game executable and every demo link this library, so a
# demo can try out game code without copying it.
# ------------------------------------------------------------------------------

# Same warnings as the engine (applied PRIVATE to game targets).
# -Werror=switch: a switch over an enum that misses a case is an error.
set(NIGHTMOURN_WARNINGS
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Werror=switch>"
    "$<$<CXX_COMPILER_ID:MSVC>:/W4;/we4062>"
)

add_library(nightmourn_game STATIC
    src/Nightmourn/Character/Character.cpp
    src/Nightmourn/Draw/CharacterDrawer.cpp
    src/Nightmourn/Draw/ChunkDrawer.cpp
    src/Nightmourn/Draw/DebugFlags.cpp
    src/Nightmourn/Draw/DrawContext.cpp
    src/Nightmourn/Draw/DrawUtils.cpp
    src/Nightmourn/Draw/UnitDrawer.cpp
    src/Nightmourn/Draw/UnitGroupDrawer.cpp
    src/Nightmourn/Draw/WorldDrawer.cpp
    src/Nightmourn/GUI/DebugMenu.cpp
    src/Nightmourn/Input/Actions.cpp
    src/Nightmourn/Player/Player.cpp
)

target_include_directories(nightmourn_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(nightmourn_game PRIVATE ${NIGHTMOURN_WARNINGS})
target_link_libraries(nightmourn_game PUBLIC Nezumi::Engine)

# ------------------------------------------------------------------------------
# Game
# ------------------------------------------------------------------------------

add_executable(nightmourn src/main.cpp)
target_link_libraries(nightmourn PRIVATE nightmourn_game)

# Copy the engine's runtime assets (shaders, fonts), then this game's assets/
# folder on top, next to the executable. Game files override engine files with
# the same path (e.g. assets/graphics/atlas.png).
nezumi_copy_assets(nightmourn ENGINE)

# ------------------------------------------------------------------------------
# Demos
# ------------------------------------------------------------------------------

if(NIGHTMOURN_BUILD_DEMOS)
    add_subdirectory(demos)
endif()

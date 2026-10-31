cmake_minimum_required(VERSION 3.25)
project(Nightmourn LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(NIGHTMOURN_BUILD_DEMOS "Build the Nightmourn demos" ON)
option(NIGHTMOURN_BUILD_TESTS "Build the Nightmourn tests" ON)

# ------------------------------------------------------------------------------
# Nezumi
#
# Fetched at configure time from Nezumi's main branch. Nezumi in turn fetches
# its own dependencies, and builds only the engine (no examples or tests)
# because it is not the top-level project here.
#
# GIT_TAG tracks `main`, so a fresh configure always gets the latest engine,
# and an engine change that renames things breaks the game until it catches
# up. To freeze the engine instead, set GIT_TAG to a commit SHA.
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
# Game code
#
# Everything the game and its demos share lives in src/Nightmourn/ and is
# included as <Nightmourn/...>, the same way the engine is included as
# <Nezumi/...>. It is built as two libraries, nightmourn_world (the headless
# simulation) and nightmourn_game (everything that needs the full engine). The
# game executable and every demo link nightmourn_game, so a demo can try out
# game code without copying it; tests link only nightmourn_world.
# ------------------------------------------------------------------------------

# Same warnings as the engine (applied PRIVATE to game targets).
# -Werror=switch: a switch over an enum that misses a case is an error.
set(NIGHTMOURN_WARNINGS
    "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Werror=switch>"
    "$<$<CXX_COMPILER_ID:MSVC>:/W4;/we4062>"
)

# ------------------------------------------------------------------------------
# World library
#
# The simulation: World, surfaces, chunks, sectors, entities and everything
# that ticks (units, resources, characters), plus items. No window, renderer,
# input or UI, so it builds and tests headless. Links only the engine's
# generic building blocks (Core, Object, Event, Logger).
# ------------------------------------------------------------------------------

add_library(nightmourn_world STATIC
    src/Nightmourn/Character/Character.cpp
    src/Nightmourn/Chunk/Chunk.cpp
    src/Nightmourn/Item/Inventory.cpp
    src/Nightmourn/Sector/Sector.cpp
    src/Nightmourn/Surface/Surface.cpp
    src/Nightmourn/Unit/Unit.cpp
    src/Nightmourn/Unit/UnitGroup.cpp
    src/Nightmourn/World/World.cpp
)

target_include_directories(nightmourn_world PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(nightmourn_world PRIVATE ${NIGHTMOURN_WARNINGS})
target_link_libraries(nightmourn_world
    PUBLIC
        nezumi_core
        nezumi_logger
        glm::glm
)

# ------------------------------------------------------------------------------
# Game library
#
# Everything on top of the world that needs the full engine: drawing, GUI,
# input and the Player.
# ------------------------------------------------------------------------------

add_library(nightmourn_game STATIC
    src/Nightmourn/Draw/CharacterDrawer.cpp
    src/Nightmourn/Draw/ChunkDrawer.cpp
    src/Nightmourn/Draw/DebugFlags.cpp
    src/Nightmourn/Draw/DrawContext.cpp
    src/Nightmourn/Draw/DrawUtils.cpp
    src/Nightmourn/Draw/ResourceDrawer.cpp
    src/Nightmourn/Draw/UnitDrawer.cpp
    src/Nightmourn/Draw/UnitGroupDrawer.cpp
    src/Nightmourn/Draw/WorldDrawer.cpp
    src/Nightmourn/GUI/ActionBar.cpp
    src/Nightmourn/GUI/DebugMenu.cpp
    src/Nightmourn/GUI/GameMenus.cpp
    src/Nightmourn/GUI/InfoOverlay.cpp
    src/Nightmourn/GUI/InventoryMenu.cpp
    src/Nightmourn/GUI/Theme.cpp
    src/Nightmourn/GUI/UIAtlas.cpp
    src/Nightmourn/Input/Actions.cpp
    src/Nightmourn/Input/Picking.cpp
    src/Nightmourn/Player/Player.cpp
)

target_include_directories(nightmourn_game PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(nightmourn_game PRIVATE ${NIGHTMOURN_WARNINGS})
target_link_libraries(nightmourn_game PUBLIC nightmourn_world Nezumi::Engine)

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

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

if(NIGHTMOURN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

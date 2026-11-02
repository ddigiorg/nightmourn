# ==============================================================================
# demos/CMakeLists.txt
#
# Small, focused prototypes of one game feature each (the game's version of
# Nezumi's examples/). A demo is where a feature is tried out before it is
# wired into the full game in src/main.cpp.
# ==============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/demos/bin)

# ------------------------------------------------------------------------------
# add_demo(DEMO_NAME)
#
# Registers a demo executable.
#
# Expects: demos/<NAME>/main.cpp
# Output:  <build>/demos/bin/<NAME>
#
# Each demo:
#   - Links the game library (and through it, Nezumi::Engine)
#   - Gets the engine's and the game's assets/ copied next to its executable
# ------------------------------------------------------------------------------

function(add_demo DEMO_NAME)
    set(DEMO_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${DEMO_NAME}/main.cpp)

    if(NOT EXISTS ${DEMO_SOURCE})
        message(WARNING "add_demo: source not found: ${DEMO_SOURCE}")
        return()
    endif()

    add_executable(${DEMO_NAME} ${DEMO_SOURCE})
    target_link_libraries(${DEMO_NAME} PRIVATE nightmourn_game)
    target_compile_options(${DEMO_NAME} PRIVATE ${NIGHTMOURN_WARNINGS})

    nezumi_copy_assets(${DEMO_NAME} ENGINE)
endfunction()

# ------------------------------------------------------------------------------
# Demos
# ------------------------------------------------------------------------------

add_demo(00-hello-world)
add_demo(01-player-armies)
add_demo(02-mining)
add_demo(03-night)

# ==============================================================================
# tests/CMakeLists.txt
#
# Headless tests of the simulation. Each links nightmourn_world only (no
# window, renderer or UI) and is registered with CTest:
#
#   ctest --test-dir .build/debug
# ==============================================================================

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/bin)

# ------------------------------------------------------------------------------
# add_world_test(TEST_NAME)
#
# Expects: tests/<NAME>/main.cpp
# Output:  <build>/tests/bin/<NAME>
# ------------------------------------------------------------------------------

function(add_world_test TEST_NAME)
    set(TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}/main.cpp)

    if(NOT EXISTS ${TEST_SOURCE})
        message(WARNING "add_world_test: source not found: ${TEST_SOURCE}")
        return()
    endif()

    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE nightmourn_world)
    target_compile_options(${TEST_NAME} PRIVATE ${NIGHTMOURN_WARNINGS})

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------

add_world_test(test_unit_group)

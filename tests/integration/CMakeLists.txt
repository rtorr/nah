add_executable(integration_tests
    cli_tests.cpp
    package_archive_tests.cpp
)

target_include_directories(integration_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(integration_tests PRIVATE
    doctest::doctest
    nlohmann_json::nlohmann_json
    NAH::package
)
target_compile_options(integration_tests PRIVATE ${NAH_WARNING_FLAGS})

# The tests need to be able to find the nah CLI executable
# Copy the CLI to the test directory for testing (preserves platform extension)
add_custom_command(TARGET integration_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nah> $<TARGET_FILE_DIR:integration_tests>/$<TARGET_FILE_NAME:nah>
)

add_test(NAME integration_tests COMMAND integration_tests)
add_test(
    NAME cli_lifecycle
    COMMAND ${CMAKE_COMMAND} -DNAH_EXE=$<TARGET_FILE:nah>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/lifecycle_test.cmake
)

# The CLI is copied beside the test executable above.
set_tests_properties(integration_tests PROPERTIES
    WORKING_DIRECTORY $<TARGET_FILE_DIR:integration_tests>
)
set_tests_properties(cli_lifecycle PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

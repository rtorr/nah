add_executable(integration_tests
    cli_tests.cpp
    package_archive_tests.cpp
)

target_include_directories(integration_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tools/nah
)

target_link_libraries(integration_tests PRIVATE
    doctest::doctest
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
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

# Set working directory to where the nah executable is located
set_tests_properties(integration_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    ENVIRONMENT "PATH=${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}"
)
set_tests_properties(cli_lifecycle PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

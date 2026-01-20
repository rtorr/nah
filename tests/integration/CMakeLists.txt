add_executable(integration_tests
    cli_tests.cpp
)

target_link_libraries(integration_tests PRIVATE
    doctest::doctest
)

# The tests need to be able to find the nah CLI executable
# Copy the CLI to the test directory for testing (preserves platform extension)
add_custom_command(TARGET integration_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:nah> $<TARGET_FILE_DIR:integration_tests>/$<TARGET_FILE_NAME:nah>
)

# Also create a simple nah.json for version checking
add_custom_command(TARGET integration_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "{\"version\": \"${NAH_VERSION}\"}" > ${CMAKE_CURRENT_BINARY_DIR}/nah.json
)

add_test(NAME integration_tests COMMAND integration_tests)

# Set working directory to where the nah executable is located
set_tests_properties(integration_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    ENVIRONMENT "PATH=${CMAKE_CURRENT_BINARY_DIR}:$ENV{PATH}"
)

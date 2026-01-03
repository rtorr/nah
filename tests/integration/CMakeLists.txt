add_executable(nah-integration-tests
    cli_tests.cpp
)

target_link_libraries(nah-integration-tests PRIVATE
    doctest::doctest
    nahhost
    nah_contract
    nah_platform
    nah_config
    nah_manifest
    nah_packaging
)
target_include_directories(nah-integration-tests PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Add a main for doctest
target_compile_definitions(nah-integration-tests PRIVATE DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN)

add_test(NAME nah-integration-tests COMMAND nah-integration-tests)

add_executable(nah-tests
    main.cpp
    semver_tests.cpp
    path_tests.cpp
    install_record_tests.cpp
    nak_record_tests.cpp
    host_profile_tests.cpp
    tlv_tests.cpp
    manifest_tests.cpp
    manifest_parse_tests.cpp
    warnings_tests.cpp
    expansion_tests.cpp
    capabilities_tests.cpp
    contract_tests.cpp
    packaging_tests.cpp
    nak_selection_tests.cpp
    trust_tests.cpp
    override_tests.cpp
    profile_resolution_tests.cpp
    compose_contract_tests.cpp
)

target_link_libraries(nah-tests PRIVATE doctest::doctest nahhost nah_contract nah_platform nah_config nah_manifest nah_packaging)
target_include_directories(nah-tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME nah-tests COMMAND nah-tests)

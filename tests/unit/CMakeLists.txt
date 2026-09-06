add_executable(nah-tests
    main.cpp
    nah_core_tests.cpp
    nah_json_tests.cpp
    nah_fs_tests.cpp
    nah_host_tests.cpp
    nah_semver_tests.cpp
    nah_digest_tests.cpp
    nah_store_tests.cpp
)

target_link_libraries(nah-tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)
target_include_directories(nah-tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_options(nah-tests PRIVATE ${NAH_WARNING_FLAGS})
add_test(NAME nah-tests COMMAND nah-tests)

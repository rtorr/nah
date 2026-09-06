add_executable(nah-tests
    main.cpp
    nah_core_tests.cpp
    nah_json_tests.cpp
    nah_fs_tests.cpp
    nah_host_tests.cpp
    nah_semver_tests.cpp
    nah_digest_tests.cpp
    nah_exec_tests.cpp
    nah_store_tests.cpp
)

target_link_libraries(nah-tests PRIVATE doctest::doctest NAH::nah NAH::package)
target_compile_options(nah-tests PRIVATE ${NAH_WARNING_FLAGS})
add_test(NAME nah-tests COMMAND nah-tests)

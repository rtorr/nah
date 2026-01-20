add_executable(nah-tests
    main.cpp
    nah_core_tests.cpp
    nah_json_tests.cpp
    nah_fs_tests.cpp
    nah_host_tests.cpp
)

target_link_libraries(nah-tests PRIVATE doctest::doctest nlohmann_json::nlohmann_json)
target_include_directories(nah-tests PRIVATE ${PROJECT_SOURCE_DIR}/include)
add_test(NAME nah-tests COMMAND nah-tests)

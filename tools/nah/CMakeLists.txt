# NAH CLI Tool

add_executable(nah
    main.cpp
    commands/init.cpp
    commands/install.cpp
    commands/uninstall.cpp
    commands/run.cpp
    commands/list.cpp
    commands/show.cpp
    commands/which.cpp
    commands/pack.cpp
    commands/host.cpp
    commands/manifest.cpp
)

target_link_libraries(nah PRIVATE
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    semver
)

target_include_directories(nah PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(nah PRIVATE cxx_std_17)

# Pass version from VERSION file to CLI tool
target_compile_definitions(nah PRIVATE NAH_VERSION="${NAH_VERSION}")

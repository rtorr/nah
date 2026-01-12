add_executable(nah tools.cpp)

target_link_libraries(nah PRIVATE CLI11::CLI11 nahhost nah_packaging nah_compose)
target_include_directories(nah PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Pass version from VERSION file to CLI tool
target_compile_definitions(nah PRIVATE NAH_VERSION="${NAH_VERSION}")

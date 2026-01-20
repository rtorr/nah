cmake_minimum_required(VERSION 3.16)
project(test_nah CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Fetch cpp-semver (required by nah_semver.h)
FetchContent_Declare(
    cpp-semver
    GIT_REPOSITORY https://github.com/z4kn4fein/cpp-semver.git
    GIT_TAG        v0.4.0
)
FetchContent_MakeAvailable(cpp-semver)

find_package(nah REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)

add_executable(test_nah test_nah.cpp)
target_link_libraries(test_nah PRIVATE nah::core nlohmann_json::nlohmann_json semver)

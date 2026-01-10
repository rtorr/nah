include(FetchContent)

# ZLIB for gzip compression
find_package(ZLIB REQUIRED)

# OpenSSL for SHA-256 hashing (used by materializer)
find_package(OpenSSL REQUIRED)

# libcurl for HTTP fetching (used by materializer)
find_package(CURL REQUIRED)

# cpp-semver for Semantic Versioning 2.0.0
# https://github.com/z4kn4fein/cpp-semver
# Always fetched - not available in package managers
FetchContent_Declare(
    cpp-semver
    GIT_REPOSITORY https://github.com/z4kn4fein/cpp-semver.git
    GIT_TAG        v0.4.0
)
FetchContent_MakeAvailable(cpp-semver)

# Try to find packages from Conan/system first, fall back to FetchContent
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# CLI11 for command-line parsing (only needed for tools)
if(NAH_ENABLE_TOOLS)
    FetchContent_Declare(
        cli11
        GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
        GIT_TAG        v2.4.2
    )
    FetchContent_MakeAvailable(cli11)
endif()

# doctest for unit testing (only needed for tests)
if(NAH_ENABLE_TESTS)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
    )
    FetchContent_MakeAvailable(doctest)
endif()

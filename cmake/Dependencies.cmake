include(FetchContent)

# ZLIB for gzip compression
find_package(ZLIB REQUIRED)

# cpp-semver for Semantic Versioning 2.0.0
# https://github.com/z4kn4fein/cpp-semver
FetchContent_Declare(
    cpp-semver
    GIT_REPOSITORY https://github.com/z4kn4fein/cpp-semver.git
    GIT_TAG        v0.4.0
)

# toml++ for TOML parsing
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)

# nlohmann/json for JSON output
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)

# CLI11 for command-line parsing
FetchContent_Declare(
    cli11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
)

# doctest for unit testing
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG        v2.4.11
)

FetchContent_MakeAvailable(cpp-semver tomlplusplus nlohmann_json cli11 doctest)

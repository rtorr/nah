include(FetchContent)

# zlib for gzip decompression (used for .nap/.nak packages)
# Always fetch to avoid system library dependencies
FetchContent_Declare(
    zlib
    GIT_REPOSITORY https://github.com/madler/zlib.git
    GIT_TAG        v1.3.1
)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "")
FetchContent_MakeAvailable(zlib)

# Create ZLIB::ZLIB alias for compatibility
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
    set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
    set(ZLIB_LIBRARIES zlibstatic)
endif()

# nlohmann_json for JSON parsing (header-only, required by nah_json.h)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

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

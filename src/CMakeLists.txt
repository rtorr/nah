# =============================================================================
# NAH Libraries - Static and Shared
# =============================================================================
#
# Build behavior:
# - Static libraries are always built (for CLI and users who prefer static)
# - Shared library is built when NAH_BUILD_SHARED=ON (default: ON)
#
# Linking:
# - CLI tool always links statically for easy distribution
# - Users can link against nahhost (static) or nahhost_shared (shared)
#
# Cross-platform notes:
# - Windows: Uses __declspec(dllexport/dllimport) via NAH_API macro
# - Linux/macOS: Uses visibility("default") for exported symbols
# - All platforms: fPIC is enabled for shared library sources

# Collect all source files for the unified shared library
set(NAH_ALL_SOURCES
    manifest/manifest.cpp
    manifest/tlv_decode.cpp
    manifest/tlv_encode.cpp
    manifest/manifest_generate.cpp
    platform/path_utils.cpp
    platform/atomic_io.cpp
    platform/section_reader.cpp
    config/install_record.cpp
    config/nak_record.cpp
    config/host_profile.cpp
    contract/contract.cpp
    contract/semver.cpp
    contract/types.cpp
    contract/warnings.cpp
    contract/expansion.cpp
    contract/capabilities.cpp
    contract/nak_selection.cpp
    packaging/packaging.cpp
    materializer/materializer.cpp
    nahhost/nahhost.cpp
)

# =============================================================================
# Static Libraries (for CLI and users who prefer static linking)
# =============================================================================

add_library(nah_manifest STATIC
    manifest/manifest.cpp
    manifest/tlv_decode.cpp
    manifest/tlv_encode.cpp
    manifest/manifest_generate.cpp
)
target_include_directories(nah_manifest PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(nah_manifest PUBLIC semver tomlplusplus::tomlplusplus)

add_library(nah_platform STATIC
    platform/path_utils.cpp
    platform/atomic_io.cpp
    platform/section_reader.cpp
)
target_include_directories(nah_platform PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

add_library(nah_config STATIC
    config/install_record.cpp
    config/nak_record.cpp
    config/host_profile.cpp
)
target_include_directories(nah_config PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(nah_config PUBLIC tomlplusplus::tomlplusplus)

add_library(nah_contract STATIC
    contract/contract.cpp
    contract/semver.cpp
    contract/types.cpp
    contract/warnings.cpp
    contract/expansion.cpp
    contract/capabilities.cpp
    contract/nak_selection.cpp
)
target_include_directories(nah_contract PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(nah_contract PUBLIC
    nah_manifest
    nah_config
    nah_platform
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    semver
)

add_library(nah_packaging STATIC
    packaging/packaging.cpp
)
target_include_directories(nah_packaging PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(nah_packaging PUBLIC
    nah_manifest
    nah_config
    nah_platform
    nah_contract
    ZLIB::ZLIB
)

add_library(nah_materializer STATIC
    materializer/materializer.cpp
)
target_include_directories(nah_materializer PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(nah_materializer PUBLIC
    nah_packaging
    nah_platform
    OpenSSL::Crypto
    CURL::libcurl
)

add_library(nahhost STATIC
    nahhost/nahhost.cpp
)
target_include_directories(nahhost PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(nahhost PUBLIC
    nah_contract
    nah_config
    nah_platform
    nah_packaging
    nah_materializer
)

# On Windows, rename static lib to avoid conflict with DLL import lib
if(WIN32)
    set_target_properties(nahhost PROPERTIES OUTPUT_NAME "nahhost_static")
endif()

# =============================================================================
# Shared Library (for users who want to update NAH without recompiling)
# =============================================================================

if(NAH_BUILD_SHARED)
    add_library(nahhost_shared SHARED ${NAH_ALL_SOURCES})

    # Set shared library properties
    set_target_properties(nahhost_shared PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        OUTPUT_NAME "nahhost"
        # Position-independent code required for shared libraries
        POSITION_INDEPENDENT_CODE ON
    )

    # Windows-specific: export all symbols or use explicit exports
    if(WIN32)
        set_target_properties(nahhost_shared PROPERTIES
            WINDOWS_EXPORT_ALL_SYMBOLS OFF
        )
    endif()

    # GCC/Clang: hide symbols by default, only export marked ones
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(nahhost_shared PRIVATE -fvisibility=hidden)
    endif()

    target_include_directories(nahhost_shared PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    target_link_libraries(nahhost_shared PUBLIC
        nlohmann_json::nlohmann_json
        tomlplusplus::tomlplusplus
        semver
        ZLIB::ZLIB
        OpenSSL::Crypto
        CURL::libcurl
    )

    # Define macros for DLL export/import
    # NAH_BUILDING_SHARED: defined when building the shared library
    # NAH_SHARED: defined by users when linking against the shared library
    target_compile_definitions(nahhost_shared
        PRIVATE NAH_BUILDING_SHARED
        PUBLIC $<BUILD_INTERFACE:NAH_SHARED>
    )
endif()

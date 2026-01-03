add_library(nah_manifest STATIC
    manifest/manifest.cpp
    manifest/tlv_decode.cpp
    manifest/tlv_encode.cpp
)
target_include_directories(nah_manifest PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_library(nah_platform STATIC
    platform/path_utils.cpp
    platform/atomic_io.cpp
    platform/section_reader.cpp
)
target_include_directories(nah_platform PUBLIC ${PROJECT_SOURCE_DIR}/include)

add_library(nah_config STATIC
    config/install_record.cpp
    config/nak_record.cpp
    config/host_profile.cpp
)
target_include_directories(nah_config PUBLIC ${PROJECT_SOURCE_DIR}/include)
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
target_include_directories(nah_contract PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(nah_contract PUBLIC
    nah_manifest
    nah_config
    nah_platform
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)

add_library(nah_packaging STATIC
    packaging/packaging.cpp
)
target_include_directories(nah_packaging PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(nah_packaging PUBLIC
    nah_manifest
    nah_config
    nah_platform
    nah_contract
    ZLIB::ZLIB
)

add_library(nahhost STATIC
    nahhost/nahhost.cpp
)
target_include_directories(nahhost PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(nahhost PUBLIC
    nah_contract
    nah_config
    nah_platform
    nah_packaging
)

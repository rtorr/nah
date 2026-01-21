# NAH App Build Template (Examples Only)
# ======================================
# Convenience CMake patterns for the NAH examples.
# This is NOT part of NAH itself - just shared code to reduce duplication in examples.
#
# v1.1.0: Updated for JSON-only manifests (no binary TLV format)
#
# Usage:
#   include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/NahAppTemplate.cmake)
#
#   nah_find_cli()  # Optional - only needed for install/run commands
#   nah_find_sdk(framework)  # or: nah_find_sdk(gameengine)
#   nah_create_app(myapp src/main.c)
#   
#   # Option 1: Explicit manifest + package
#   nah_app_manifest(myapp
#       ID "com.example.myapp"
#       VERSION "1.0.0"
#       NAK_ID "com.example.sdk"
#       NAK_VERSION "^1.0.0"
#       ENTRYPOINT "bin/myapp"
#   )
#   nah_package(myapp ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
#   
#   # Option 2: Shorthand (combines above)
#   nah_app(myapp
#       ID "com.example.myapp"
#       VERSION "1.0.0"
#       NAK "com.example.sdk"
#       NAK_VERSION "^1.0.0"
#       ENTRYPOINT "bin/myapp"
#       ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/assets"
#   )

# Find NAH CLI
# Sets: NAH_CLI
# Note: CLI is now optional for pure CMake workflows (manifest generation is built-in)
macro(nah_find_cli)
    if(NOT DEFINED NAH_CLI)
        find_program(NAH_CLI nah
            PATHS
                /usr/local/bin
                /usr/bin
                $ENV{HOME}/.local/bin
        )
    endif()

    if(NOT NAH_CLI)
        # Fallback for examples tree (development convenience only)
        set(_nah_examples_cli "${CMAKE_CURRENT_SOURCE_DIR}/../../build/tools/nah/nah")
        set(_nah_root_cli "${CMAKE_CURRENT_SOURCE_DIR}/../../../build/tools/nah/nah")

        if(EXISTS "${_nah_examples_cli}")
            set(NAH_CLI "${_nah_examples_cli}")
            message(STATUS "Using NAH CLI from examples tree (development mode)")
        elseif(EXISTS "${_nah_root_cli}")
            set(NAH_CLI "${_nah_root_cli}")
            message(STATUS "Using NAH CLI from project root (development mode)")
        else()
            message(STATUS "NAH CLI not found (optional for pure CMake builds)")
        endif()
    else()
        message(STATUS "NAH CLI: ${NAH_CLI}")
    endif()
endmacro()

# Find SDK (framework or gameengine)
# Sets: NAH_SDK_INCLUDE_DIR, NAH_SDK_LIB_DIR, NAH_SDK_LIBRARY
macro(nah_find_sdk SDK_NAME)
    set(_sdk_include_var "${SDK_NAME}_SDK_INCLUDE_DIR")
    set(_sdk_lib_var "${SDK_NAME}_SDK_LIB_DIR")
    set(_sdk_dir_var "${SDK_NAME}_SDK_DIR")

    # Check for explicit SDK_DIR
    if(DEFINED ${_sdk_dir_var})
        set(NAH_SDK_INCLUDE_DIR "${${_sdk_dir_var}}/../include")
        set(NAH_SDK_LIB_DIR "${${_sdk_dir_var}}")
        message(STATUS "Using ${SDK_NAME} SDK from: ${${_sdk_dir_var}}")
        # Check for explicit INCLUDE_DIR and LIB_DIR
    elseif(DEFINED ${_sdk_include_var} AND DEFINED ${_sdk_lib_var})
        set(NAH_SDK_INCLUDE_DIR "${${_sdk_include_var}}")
        set(NAH_SDK_LIB_DIR "${${_sdk_lib_var}}")
        message(STATUS "Using ${SDK_NAME} SDK includes: ${NAH_SDK_INCLUDE_DIR}")
        message(STATUS "Using ${SDK_NAME} SDK libs: ${NAH_SDK_LIB_DIR}")
    else()
        # Try system paths first
        if("${SDK_NAME}" STREQUAL "framework")
            set(_header "framework/framework.h")
            set(_examples_include "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/include")
            set(_examples_lib "${CMAKE_CURRENT_SOURCE_DIR}/../../sdk/build")
        elseif("${SDK_NAME}" STREQUAL "gameengine")
            set(_header "sdk/engine.hpp")
            set(_examples_include "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/include")
            set(_examples_lib "${CMAKE_CURRENT_SOURCE_DIR}/../../conan-sdk/build/build/Release")
        else()
            message(FATAL_ERROR "Unknown SDK: ${SDK_NAME}")
        endif()

        find_path(NAH_SDK_INCLUDE_DIR ${_header}
            PATHS
                /usr/local/include
                /usr/include
        )
        find_library(NAH_SDK_LIBRARY ${SDK_NAME}
            PATHS
                /usr/local/lib
                /usr/lib
        )

        # Fallback for examples tree
        if(NOT NAH_SDK_INCLUDE_DIR AND EXISTS "${_examples_include}")
            set(NAH_SDK_INCLUDE_DIR "${_examples_include}")
            set(NAH_SDK_LIB_DIR "${_examples_lib}")
            message(STATUS "Using ${SDK_NAME} SDK from examples tree (development mode)")
        endif()
    endif()

    set(NAH_SDK_NAME "${SDK_NAME}")
endmacro()

# Create app executable with SDK linkage
# Usage: nah_create_app(target_name source1 source2 ...)
function(nah_create_app TARGET_NAME)
    set(SOURCES ${ARGN})

    add_executable(${TARGET_NAME} ${SOURCES})

    if(NAH_SDK_INCLUDE_DIR)
        target_include_directories(${TARGET_NAME} PRIVATE ${NAH_SDK_INCLUDE_DIR})
    endif()

    if(NAH_SDK_LIB_DIR AND EXISTS "${NAH_SDK_LIB_DIR}")
        target_link_directories(${TARGET_NAME} PRIVATE ${NAH_SDK_LIB_DIR})
        target_link_libraries(${TARGET_NAME} PRIVATE ${NAH_SDK_NAME})
    elseif(NAH_SDK_LIBRARY)
        target_link_libraries(${TARGET_NAME} PRIVATE ${NAH_SDK_LIBRARY})
    endif()
endfunction()

################################################################################
# Generate app manifest (nap.json)
# Usage: nah_app_manifest(target_name
#            ID "com.example.app"
#            VERSION "1.0.0"
#            [NAK_ID "com.example.sdk"]
#            [NAK_VERSION "^1.0.0"]
#            ENTRYPOINT "bin/app"
#            [LIB_DIRS "lib" "lib/x64"]
#            [ASSET_DIRS "assets" "share"]
#            [ENV_VARS KEY1=VALUE1 KEY2=VALUE2]
#        )
################################################################################
function(nah_app_manifest TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;NAK_ID;NAK_VERSION;ENTRYPOINT" 
        "LIB_DIRS;ASSET_DIRS;ENV_VARS;EXPORTS" 
        ${ARGN}
    )

    if(NOT ARG_ID OR NOT ARG_VERSION OR NOT ARG_ENTRYPOINT)
        message(FATAL_ERROR "nah_app_manifest requires ID, VERSION, and ENTRYPOINT")
    endif()

    # Build JSON manifest
    set(MANIFEST_JSON "{\n")
    string(APPEND MANIFEST_JSON "  \"$schema\": \"https://nah.rtorr.com/schemas/nap.v1.json\",\n")
    string(APPEND MANIFEST_JSON "  \"app\": {\n")
    string(APPEND MANIFEST_JSON "    \"identity\": {\n")
    string(APPEND MANIFEST_JSON "      \"id\": \"${ARG_ID}\",\n")
    string(APPEND MANIFEST_JSON "      \"version\": \"${ARG_VERSION}\"")
    
    if(ARG_NAK_ID)
        string(APPEND MANIFEST_JSON ",\n      \"nak_id\": \"${ARG_NAK_ID}\"")
    endif()
    if(ARG_NAK_VERSION)
        string(APPEND MANIFEST_JSON ",\n      \"nak_version_req\": \"${ARG_NAK_VERSION}\"")
    endif()
    
    string(APPEND MANIFEST_JSON "\n    },\n")
    string(APPEND MANIFEST_JSON "    \"execution\": {\n")
    string(APPEND MANIFEST_JSON "      \"entrypoint\": \"${ARG_ENTRYPOINT}\"\n")
    string(APPEND MANIFEST_JSON "    }")
    
    # Add layout if lib_dirs or asset_dirs provided
    if(ARG_LIB_DIRS OR ARG_ASSET_DIRS)
        string(APPEND MANIFEST_JSON ",\n    \"layout\": {\n")
        
        if(ARG_LIB_DIRS)
            string(APPEND MANIFEST_JSON "      \"lib_dirs\": [")
            set(_first TRUE)
            foreach(_dir ${ARG_LIB_DIRS})
                if(_first)
                    set(_first FALSE)
                else()
                    string(APPEND MANIFEST_JSON ", ")
                endif()
                string(APPEND MANIFEST_JSON "\"${_dir}\"")
            endforeach()
            string(APPEND MANIFEST_JSON "]")
        endif()
        
        if(ARG_ASSET_DIRS)
            if(ARG_LIB_DIRS)
                string(APPEND MANIFEST_JSON ",\n")
            endif()
            string(APPEND MANIFEST_JSON "      \"asset_dirs\": [")
            set(_first TRUE)
            foreach(_dir ${ARG_ASSET_DIRS})
                if(_first)
                    set(_first FALSE)
                else()
                    string(APPEND MANIFEST_JSON ", ")
                endif()
                string(APPEND MANIFEST_JSON "\"${_dir}\"")
            endforeach()
            string(APPEND MANIFEST_JSON "]")
        endif()
        
        string(APPEND MANIFEST_JSON "\n    }")
    endif()
    
    # Add environment if provided
    if(ARG_ENV_VARS)
        string(APPEND MANIFEST_JSON ",\n    \"environment\": {\n")
        set(_first TRUE)
        foreach(_var ${ARG_ENV_VARS})
            if(_first)
                set(_first FALSE)
            else()
                string(APPEND MANIFEST_JSON ",\n")
            endif()
            string(REGEX MATCH "([^=]+)=(.*)" _matched "${_var}")
            string(APPEND MANIFEST_JSON "      \"${CMAKE_MATCH_1}\": \"${CMAKE_MATCH_2}\"")
        endforeach()
        string(APPEND MANIFEST_JSON "\n    }")
    endif()
    
    string(APPEND MANIFEST_JSON "\n  }\n")
    string(APPEND MANIFEST_JSON "}\n")

    # Write manifest file
    set(MANIFEST_FILE "${CMAKE_BINARY_DIR}/${TARGET_NAME}_nap.json")
    file(WRITE ${MANIFEST_FILE} "${MANIFEST_JSON}")
    
    # Set parent scope variables for nah_package
    set(${TARGET_NAME}_MANIFEST_FILE ${MANIFEST_FILE} PARENT_SCOPE)
    set(${TARGET_NAME}_APP_ID ${ARG_ID} PARENT_SCOPE)
    set(${TARGET_NAME}_APP_VERSION ${ARG_VERSION} PARENT_SCOPE)
    
    message(STATUS "Generated app manifest: ${MANIFEST_FILE}")
endfunction()

################################################################################
# Generate NAK manifest (nak.json)
# Usage: nah_nak_manifest(target_name
#            ID "com.example.sdk"
#            VERSION "1.0.0"
#            [LOADER "bin/loader"]
#            [LOADER_ARGS "--app-root" "{NAH_APP_ROOT}"]
#            [LIB_DIRS "lib"]
#            [RESOURCE_ROOT "resources"]
#        )
################################################################################
function(nah_nak_manifest TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;LOADER;RESOURCE_ROOT;CWD" 
        "LOADER_ARGS;LIB_DIRS;ENV_VARS" 
        ${ARGN}
    )

    if(NOT ARG_ID OR NOT ARG_VERSION)
        message(FATAL_ERROR "nah_nak_manifest requires ID and VERSION")
    endif()

    # Build JSON manifest
    set(MANIFEST_JSON "{\n")
    string(APPEND MANIFEST_JSON "  \"$schema\": \"https://nah.rtorr.com/schemas/nak.v1.json\",\n")
    string(APPEND MANIFEST_JSON "  \"nak\": {\n")
    string(APPEND MANIFEST_JSON "    \"identity\": {\n")
    string(APPEND MANIFEST_JSON "      \"id\": \"${ARG_ID}\",\n")
    string(APPEND MANIFEST_JSON "      \"version\": \"${ARG_VERSION}\"\n")
    string(APPEND MANIFEST_JSON "    },\n")
    string(APPEND MANIFEST_JSON "    \"paths\": {\n")
    
    if(ARG_RESOURCE_ROOT)
        string(APPEND MANIFEST_JSON "      \"resource_root\": \"${ARG_RESOURCE_ROOT}\"")
    else()
        string(APPEND MANIFEST_JSON "      \"resource_root\": \"resources\"")
    endif()
    
    if(ARG_LIB_DIRS)
        string(APPEND MANIFEST_JSON ",\n      \"lib_dirs\": [")
        set(_first TRUE)
        foreach(_dir ${ARG_LIB_DIRS})
            if(_first)
                set(_first FALSE)
            else()
                string(APPEND MANIFEST_JSON ", ")
            endif()
            string(APPEND MANIFEST_JSON "\"${_dir}\"")
        endforeach()
        string(APPEND MANIFEST_JSON "]")
    endif()
    
    string(APPEND MANIFEST_JSON "\n    }")
    
    # Add environment if provided
    if(ARG_ENV_VARS)
        string(APPEND MANIFEST_JSON ",\n    \"environment\": {\n")
        set(_first TRUE)
        foreach(_var ${ARG_ENV_VARS})
            if(_first)
                set(_first FALSE)
            else()
                string(APPEND MANIFEST_JSON ",\n")
            endif()
            string(REGEX MATCH "([^=]+)=(.*)" _matched "${_var}")
            string(APPEND MANIFEST_JSON "      \"${CMAKE_MATCH_1}\": \"${CMAKE_MATCH_2}\"")
        endforeach()
        string(APPEND MANIFEST_JSON "\n    }")
    endif()
    
    # Add loader if provided
    if(ARG_LOADER)
        string(APPEND MANIFEST_JSON ",\n    \"loader\": {\n")
        string(APPEND MANIFEST_JSON "      \"exec_path\": \"${ARG_LOADER}\"")
        
        if(ARG_LOADER_ARGS)
            string(APPEND MANIFEST_JSON ",\n      \"args_template\": [")
            set(_first TRUE)
            foreach(_arg ${ARG_LOADER_ARGS})
                if(_first)
                    set(_first FALSE)
                else()
                    string(APPEND MANIFEST_JSON ", ")
                endif()
                string(APPEND MANIFEST_JSON "\"${_arg}\"")
            endforeach()
            string(APPEND MANIFEST_JSON "]")
        endif()
        
        string(APPEND MANIFEST_JSON "\n    }")
    endif()
    
    # Add execution context if provided
    if(ARG_CWD)
        string(APPEND MANIFEST_JSON ",\n    \"execution\": {\n")
        string(APPEND MANIFEST_JSON "      \"cwd\": \"${ARG_CWD}\"\n")
        string(APPEND MANIFEST_JSON "    }")
    endif()
    
    string(APPEND MANIFEST_JSON "\n  }\n")
    string(APPEND MANIFEST_JSON "}\n")

    # Write manifest file
    set(MANIFEST_FILE "${CMAKE_BINARY_DIR}/${TARGET_NAME}_nak.json")
    file(WRITE ${MANIFEST_FILE} "${MANIFEST_JSON}")
    
    # Set parent scope variables
    set(${TARGET_NAME}_MANIFEST_FILE ${MANIFEST_FILE} PARENT_SCOPE)
    set(${TARGET_NAME}_NAK_ID ${ARG_ID} PARENT_SCOPE)
    set(${TARGET_NAME}_NAK_VERSION ${ARG_VERSION} PARENT_SCOPE)
    
    message(STATUS "Generated NAK manifest: ${MANIFEST_FILE}")
endfunction()

################################################################################
# Package app or NAK as tar.gz
# Usage: nah_package(target_name [ASSETS_DIR dir])
# Automatically detects type from manifest file
################################################################################
function(nah_package TARGET_NAME)
    cmake_parse_arguments(ARG "" "ASSETS_DIR" "" ${ARGN})

    if(NOT DEFINED ${TARGET_NAME}_MANIFEST_FILE)
        message(FATAL_ERROR "No manifest generated for ${TARGET_NAME}. Call nah_app_manifest or nah_nak_manifest first.")
    endif()

    # Detect package type from manifest filename
    string(REGEX MATCH "_([^_]+)\\.json$" _match "${${TARGET_NAME}_MANIFEST_FILE}")
    set(PACKAGE_TYPE "${CMAKE_MATCH_1}")  # "nap" or "nak"
    
    if(PACKAGE_TYPE STREQUAL "nap")
        set(PKG_ID "${${TARGET_NAME}_APP_ID}")
        set(PKG_VERSION "${${TARGET_NAME}_APP_VERSION}")
        set(MANIFEST_NAME "nap.json")
    elseif(PACKAGE_TYPE STREQUAL "nak")
        set(PKG_ID "${${TARGET_NAME}_NAK_ID}")
        set(PKG_VERSION "${${TARGET_NAME}_NAK_VERSION}")
        set(MANIFEST_NAME "nak.json")
    else()
        message(FATAL_ERROR "Unknown package type for ${TARGET_NAME}")
    endif()

    set(STAGING_DIR "${CMAKE_BINARY_DIR}/${TARGET_NAME}_staging")
    set(PACKAGE_FILE "${CMAKE_BINARY_DIR}/${PKG_ID}-${PKG_VERSION}.${PACKAGE_TYPE}")

    # Create staging directory structure
    add_custom_target(${TARGET_NAME}_stage
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${STAGING_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/bin
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/lib
        COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/assets
        
        # Copy binary
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${STAGING_DIR}/bin/
        
        # Copy manifest with correct name
        COMMAND ${CMAKE_COMMAND} -E copy ${${TARGET_NAME}_MANIFEST_FILE} ${STAGING_DIR}/${MANIFEST_NAME}
        
        DEPENDS ${TARGET_NAME}
        COMMENT "Staging ${PACKAGE_TYPE}: ${PKG_ID}@${PKG_VERSION}"
    )

    # Copy assets if provided
    if(ARG_ASSETS_DIR AND EXISTS "${ARG_ASSETS_DIR}")
        add_custom_command(TARGET ${TARGET_NAME}_stage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${ARG_ASSETS_DIR} ${STAGING_DIR}/assets
            COMMENT "Copying assets from ${ARG_ASSETS_DIR}"
        )
    endif()

    # Package using tar with deterministic flags
    add_custom_target(${TARGET_NAME}_package ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Creating ${PACKAGE_TYPE} package: ${PACKAGE_FILE}"
        COMMAND ${CMAKE_COMMAND} -E tar czf ${PACKAGE_FILE} --format=gnutar .
        WORKING_DIRECTORY ${STAGING_DIR}
        DEPENDS ${TARGET_NAME}_stage
        COMMENT "Packaging ${PACKAGE_TYPE}: ${PKG_ID}-${PKG_VERSION}.${PACKAGE_TYPE}"
        BYPRODUCTS ${PACKAGE_FILE}
    )

    # Add to global package_all target
    if(NOT TARGET package_all)
        add_custom_target(package_all)
    endif()
    add_dependencies(package_all ${TARGET_NAME}_package)

    message(STATUS "Package target: ${TARGET_NAME}_package → ${PACKAGE_FILE}")
endfunction()

################################################################################
# Convenience wrapper: nah_app() - Declare + package in one call
################################################################################
function(nah_app TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;NAK;NAK_VERSION;ENTRYPOINT;ASSETS" 
        "ENV_VARS" 
        ${ARGN}
    )

    # Generate manifest
    nah_app_manifest(${TARGET_NAME}
        ID ${ARG_ID}
        VERSION ${ARG_VERSION}
        NAK_ID ${ARG_NAK}
        NAK_VERSION ${ARG_NAK_VERSION}
        ENTRYPOINT ${ARG_ENTRYPOINT}
        ENV_VARS ${ARG_ENV_VARS}
    )

    # Package
    if(ARG_ASSETS)
        nah_package(${TARGET_NAME} ASSETS_DIR ${ARG_ASSETS})
    else()
        nah_package(${TARGET_NAME})
    endif()
endfunction()

################################################################################
# Convenience wrapper: nah_nak() - Declare + package in one call
################################################################################
function(nah_nak TARGET_NAME)
    cmake_parse_arguments(ARG 
        "" 
        "ID;VERSION;LOADER;CWD" 
        "LOADER_ARGS;ENV_VARS" 
        ${ARGN}
    )

    # Generate manifest
    nah_nak_manifest(${TARGET_NAME}
        ID ${ARG_ID}
        VERSION ${ARG_VERSION}
        LOADER ${ARG_LOADER}
        LOADER_ARGS ${ARG_LOADER_ARGS}
        CWD ${ARG_CWD}
        ENV_VARS ${ARG_ENV_VARS}
    )

    # Package
    nah_package(${TARGET_NAME})
endfunction()

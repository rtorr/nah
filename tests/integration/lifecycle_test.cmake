if(NOT DEFINED NAH_EXE)
    message(FATAL_ERROR "NAH_EXE is required")
endif()

set(work "${CMAKE_CURRENT_BINARY_DIR}/lifecycle test")
set(app "${work}/app project")
set(root "${work}/nah root")
set(package "${work}/app package.nap")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}")

function(run_nah)
    execute_process(
        COMMAND "${NAH_EXE}" ${ARGN}
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "nah ${ARGN} failed (${status})\nstdout: ${output}\nstderr: ${error}")
    endif()
endfunction()

run_nah(init --app --id com.example.lifecycle "${app}")
if(UNIX)
    file(WRITE "${app}/bin/app" "#!/bin/sh\nprintf '%s\\n' \"$@\"\n")
    file(CHMOD "${app}/bin/app" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()
run_nah(pack "${app}" --output "${package}")
file(SHA256 "${package}" package_sha256)
execute_process(
    COMMAND "${NAH_EXE}" --root "${work}/mismatch root" install "${package}"
            --expected-sha256 "0000000000000000000000000000000000000000000000000000000000000000"
    RESULT_VARIABLE mismatch_status OUTPUT_QUIET ERROR_QUIET
)
if(mismatch_status EQUAL 0 OR EXISTS "${work}/mismatch root/apps/com.example.lifecycle-0.1.0")
    message(FATAL_ERROR "install accepted an incorrect expected digest")
endif()
run_nah(--root "${root}" install "${package}" --expected-sha256 "${package_sha256}")
run_nah(--root "${root}" which com.example.lifecycle@0.1.0)
run_nah(--root "${root}" show com.example.lifecycle)
if(UNIX)
    execute_process(
        COMMAND "${NAH_EXE}" --root "${root}" run com.example.lifecycle --require-verified -- first "two words"
        RESULT_VARIABLE run_status OUTPUT_VARIABLE run_output ERROR_VARIABLE run_error
    )
    if(NOT run_status EQUAL 0 OR NOT run_output MATCHES "first" OR NOT run_output MATCHES "two words")
        message(FATAL_ERROR "run did not preserve arguments\nstdout: ${run_output}\nstderr: ${run_error}")
    endif()
endif()
set(record "${root}/registry/apps/com.example.lifecycle@0.1.0.json")
file(READ "${record}" valid_record)
string(JSON recorded_hash GET "${valid_record}" provenance package_hash)
string(JSON recorded_trust GET "${valid_record}" trust state)
if(NOT recorded_hash STREQUAL "sha256:${package_sha256}" OR NOT recorded_trust STREQUAL "verified")
    message(FATAL_ERROR "install did not persist verified package identity")
endif()
file(MAKE_DIRECTORY "${work}/outside")
file(WRITE "${work}/outside/marker" "must survive")
string(JSON unsafe_record SET "${valid_record}" paths install_root "\"../../outside\"")
file(WRITE "${record}" "${unsafe_record}")
execute_process(
    COMMAND "${NAH_EXE}" --root "${root}" uninstall com.example.lifecycle
    RESULT_VARIABLE unsafe_status
)
if(unsafe_status EQUAL 0 OR NOT EXISTS "${work}/outside/marker")
    message(FATAL_ERROR "uninstall accepted an escaping registry path")
endif()
file(WRITE "${record}" "${valid_record}")

file(CREATE_LINK "${work}/outside" "${root}/apps/link" SYMBOLIC RESULT symlink_result)
if(symlink_result STREQUAL "0")
    string(JSON symlink_record SET "${valid_record}" paths install_root "\"apps/link\"")
    file(WRITE "${record}" "${symlink_record}")
    execute_process(
        COMMAND "${NAH_EXE}" --root "${root}" uninstall com.example.lifecycle
        RESULT_VARIABLE symlink_status
        OUTPUT_QUIET ERROR_QUIET
    )
    if(symlink_status EQUAL 0 OR NOT EXISTS "${work}/outside/marker")
        message(FATAL_ERROR "uninstall followed a symlink outside the NAH root")
    endif()
    file(WRITE "${record}" "${valid_record}")
endif()

run_nah(--root "${root}" uninstall com.example.lifecycle)
if(EXISTS "${root}/apps/com.example.lifecycle-0.1.0")
    message(FATAL_ERROR "uninstall left the app directory behind")
endif()
file(REMOVE_RECURSE "${work}")

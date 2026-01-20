# bin2header.cmake
# =================
# Converts a binary file to C include files for embedding.
#
# Usage:
#   cmake -DINPUT_FILE=manifest.nah -DOUTPUT_FILE=manifest_data.h -P bin2header.cmake
#
# Output files:
#   - manifest_data.h: Header with NAH_MANIFEST_SIZE constant
#   - manifest_bytes.inc: Raw byte array initializer for #include in array

if(NOT INPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE not specified")
endif()

if(NOT OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE not specified")
endif()

# Read binary file as hex
file(READ "${INPUT_FILE}" BINARY_DATA HEX)

# Get file size
file(SIZE "${INPUT_FILE}" FILE_SIZE)

# Convert hex string to comma-separated byte values
string(LENGTH "${BINARY_DATA}" HEX_LENGTH)

# Build the byte array string
set(BYTE_ARRAY "")
set(BYTE_COUNT 0)

# Process 2 hex chars at a time
set(i 0)
while(i LESS HEX_LENGTH)
    string(SUBSTRING "${BINARY_DATA}" ${i} 2 BYTE_HEX)

    # Add comma separator after first byte
    if(BYTE_COUNT GREATER 0)
        string(APPEND BYTE_ARRAY ", ")
    endif()

    # Add newline every 12 bytes for readability
    math(EXPR MOD_12 "${BYTE_COUNT} % 12")
    if(MOD_12 EQUAL 0)
        if(BYTE_COUNT GREATER 0)
            string(APPEND BYTE_ARRAY "\n")
        endif()
        string(APPEND BYTE_ARRAY "    ")
    endif()

    string(APPEND BYTE_ARRAY "0x${BYTE_HEX}")

    math(EXPR BYTE_COUNT "${BYTE_COUNT} + 1")
    math(EXPR i "${i} + 2")
endwhile()

# Get output directory for .inc file
get_filename_component(OUTPUT_DIR "${OUTPUT_FILE}" DIRECTORY)
set(BYTES_INC "${OUTPUT_DIR}/manifest_bytes.inc")

# Write .inc file (just the bytes for array initializer)
file(WRITE "${BYTES_INC}" "// Auto-generated - DO NOT EDIT (${FILE_SIZE} bytes)\n${BYTE_ARRAY}\n")

# Generate header content
set(HEADER_CONTENT "// Auto-generated manifest data - DO NOT EDIT
// Generated from: ${INPUT_FILE}
// Size: ${FILE_SIZE} bytes

#pragma once

#include <cstddef>

// Manifest size
static const size_t NAH_MANIFEST_SIZE = ${FILE_SIZE};
")

# Write header file
file(WRITE "${OUTPUT_FILE}" "${HEADER_CONTENT}")

message(STATUS "Generated ${OUTPUT_FILE} and ${BYTES_INC} (${FILE_SIZE} bytes)")

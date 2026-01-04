cmake_minimum_required(VERSION 3.21)
project(nah VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global options
option(NAH_ENABLE_TESTS "Build NAH tests" ON)
option(NAH_ENABLE_WARNINGS "Enable strict warnings" ON)
option(NAH_ENABLE_SANITIZERS "Enable address/undefined sanitizers" OFF)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Dependencies.cmake)

# Common compile options
if(NAH_ENABLE_WARNINGS)
    if(MSVC)
        add_compile_options(/W4 /permissive- /wd4324)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
    endif()
endif()

# Sanitizers (opt-in)
if(NAH_ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(src)
add_subdirectory(tools)

if(NAH_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation
include(GNUInstallDirs)

install(TARGETS nah
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/nah
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_TOOLCHAIN_FILE)
    if(NOT DEFINED ENV{VCPKG_ROOT} OR "$ENV{VCPKG_ROOT}" STREQUAL "")
        message(FATAL_ERROR
            "VCPKG_ROOT must be set on the build machine, or pass a valid "
            "CMAKE_TOOLCHAIN_FILE.")
    endif()

    file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" GLACIER_VCPKG_ROOT)
    set(CMAKE_TOOLCHAIN_FILE
        "${GLACIER_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH "CMake toolchain file")
endif()

if(NOT CMAKE_TOOLCHAIN_FILE OR NOT EXISTS "${CMAKE_TOOLCHAIN_FILE}")
    message(FATAL_ERROR
        "The vcpkg toolchain file was not found: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET x64-windows-static CACHE STRING
        "vcpkg target triplet")
endif()

project(Glacier LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Glacier currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    else()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
endif()

set(GLACIER_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin" CACHE PATH
    "Root directory for Glacier runtime outputs")

foreach(configuration Debug Release)
    string(TOUPPER "${configuration}" configuration_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${configuration_upper}
        "${GLACIER_OUTPUT_DIRECTORY}/${configuration}")
endforeach()

add_subdirectory(renderer)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT renderer)
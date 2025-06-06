# Copyright (C) 2018-2024 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp" "*.h")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})

add_library(ie_samples_utils STATIC EXCLUDE_FROM_ALL ${SOURCES})
set_target_properties(ie_samples_utils PROPERTIES FOLDER "src")

target_include_directories(ie_samples_utils
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

find_package(OpenVINO REQUIRED COMPONENTS Runtime)

if(TARGET gflags)
    set(GFLAGS_TARGET gflags)
else()
    if(EXISTS /etc/debian_version)
        set(gflags_component nothreads_static)
    else()
        find_package(gflags QUIET OPTIONAL_COMPONENTS nothreads_static)
        if(NOT gflags_FOUND)
            set(gflags_component shared)
        else()
            set(gflags_component nothreads_static)
        endif()
    endif()
    find_package(gflags QUIET OPTIONAL_COMPONENTS ${gflags_component})
    if(gflags_FOUND)
        if(TARGET ${GFLAGS_TARGET})
            # nothing
        elseif(TARGET gflags_nothreads-static)
            # Debian 9: gflag_component is ignored
            set(GFLAGS_TARGET gflags_nothreads-static)
        elseif(TARGET gflags-shared)
            # gflags shared case for CentOS / RHEL / Fedora
            set(GFLAGS_TARGET gflags-shared)
        else()
            message(FATAL_ERROR "Internal error: failed to find imported target 'gflags' using '${gflags_component}' component")
        endif()

        message(STATUS "gflags (${gflags_VERSION}) is found at ${gflags_DIR} using '${gflags_component}' component")
    endif()

    if(NOT gflags_FOUND)
        if(EXISTS "$ENV{INTEL_OPENVINO_DIR}/samples/cpp/thirdparty/gflags")
            add_subdirectory("$ENV{INTEL_OPENVINO_DIR}/samples/cpp/thirdparty/gflags" "${CMAKE_CURRENT_BINARY_DIR}/gflag")
            set(GFLAGS_TARGET gflags_nothreads_static)
        else()
            message(FATAL_ERROR "Failed to find 'gflags' library using '${gflags_component}' component")
        endif()
    endif()
endif()

target_link_libraries(ie_samples_utils PUBLIC openvino::runtime ${GFLAGS_TARGET})

if(COMMAND add_clang_format_target)
    add_clang_format_target(ie_samples_utils_clang FOR_TARGETS ie_samples_utils)
endif()

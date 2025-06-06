# Copyright (C) 2018-2024 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

set (CMAKE_CXX_STANDARD 11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel"))
        set (CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")
endif()


file (GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/classification_sample_async.h
)

source_group("src" FILES ${MAIN_SRC})
source_group("include" FILES ${MAIN_HEADERS})

# Find OpenCV components if exist
find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc videoio REQUIRED)

# Create library file from sources.
add_executable(classification_sample_async ${MAIN_SRC} ${MAIN_HEADERS})

if (NOT WIN32)
    set (LIB_DL dl)
endif()

target_include_directories(classification_sample_async PRIVATE
    # Demo utils
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/demo_utils/include/utils
    # FPGA plugin configs
    $ENV{COREDLA_ROOT}/dla_plugin/inc
)

target_link_libraries(classification_sample_async PRIVATE
    coreDLAHeteroPlugin
    openvino::runtime
    ${OpenCV_LIBRARIES}
    format_reader
    ie_samples_utils
)

if(NOT WIN32)
    target_link_libraries(classification_sample_async PRIVATE ${LIB_DL} pthread)
endif()

set_target_properties(classification_sample_async PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

# For libcoreDlaRuntimePlugin.so - typically specified by $COREDLA_ROOT/runtime/plugins.xml
set_target_properties(classification_sample_async PROPERTIES BUILD_RPATH "\$ORIGIN/..")

install(TARGETS classification_sample_async RUNTIME DESTINATION "dla/bin" COMPONENT DEMO)
install(TARGETS classification_sample_async RUNTIME DESTINATION "dla/not_shipped/bin" COMPONENT NOT_SHIPPED)

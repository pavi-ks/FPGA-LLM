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
    # Add header files here
)

source_group("src" FILES ${MAIN_SRC})
source_group("include" FILES ${MAIN_HEADERS})

# Find OpenCV components if exist
find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc videoio REQUIRED)

# Create library file from sources.
add_executable(segmentation_demo ${MAIN_SRC} ${MAIN_HEADERS})

target_include_directories(segmentation_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/monitors/include
    $ENV{COREDLA_ROOT}/dla_plugin/inc/
)

if (NOT WIN32)
    set (LIB_DL dl)
endif()

if (NOT TARGET ie_samples_utils)
  message (FATAL_ERROR "ie_samples_utils missing")
endif()

target_link_libraries(segmentation_demo
    openvino::runtime
    coreDLAHeteroPlugin
    openvino_dev_api
    ie_samples_utils
    ${OpenCV_LIBRARIES}
    monitors
    ov_demo_utils
    models
    pipelines
)

if(NOT WIN32)
    target_link_libraries(segmentation_demo ${LIB_DL} pthread)
endif()

set_target_properties(segmentation_demo PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

# For libcoreDlaRuntimePlugin.so - typically specified by $COREDLA_ROOT/runtime/plugins.xml
set_target_properties(segmentation_demo PROPERTIES BUILD_RPATH "\$ORIGIN/..")

install(TARGETS segmentation_demo RUNTIME DESTINATION "dla/bin" COMPONENT DEMO)
install(TARGETS segmentation_demo RUNTIME DESTINATION "dla/not_shipped/bin" COMPONENT NOT_SHIPPED)

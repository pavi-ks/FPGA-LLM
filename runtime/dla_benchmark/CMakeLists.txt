# Copyright (C) 2018-2020 Altera Corporation
# SPDX-License-Identifier: Apache-2.0
#

if (CMAKE_CXX_COMPILER_ID MATCHES MSVC)
  set(CMAKE_CXX_STANDARD 20)
else()
  set (CMAKE_CXX_STANDARD 14)
endif()
set (CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT WIN32)
  if (NOT("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel"))
    set (CMAKE_CXX_FLAGS "-std=c++14 ${CMAKE_CXX_FLAGS}")
  endif()
endif()

file (GLOB MAIN_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/utils/src/*.cpp
)

file (GLOB MAIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

# Create named folders for the sources within the .vcproj
# Empty name lists them directly under the .vcproj
source_group("src" FILES ${MAIN_SRC})
source_group("include" FILES ${MAIN_HEADERS})

if (DE10_AGILEX)
  add_definitions(-DDE10_AGILEX)
endif()

# Find OpenCV components if exist
find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc videoio REQUIRED)

# Create library file from sources.
add_executable(dla_benchmark ${MAIN_SRC} ${MAIN_HEADERS})

# For FPGA plugin configs and properties.
target_include_directories(dla_benchmark PRIVATE
    "$ENV{COREDLA_ROOT}/dla_plugin/inc/"
)

if (NOT WIN32)
    set (LIB_DL dl)
endif()

target_link_libraries(dla_benchmark PRIVATE
  openvino::runtime
  openvino_dev_api
  ${OpenCV_LIBRARIES}
  coreDLAHeteroPlugin
  format_reader
  ie_samples_utils
  dla_util
)

if (NOT WIN32)
    target_link_libraries(dla_benchmark PRIVATE ${LIB_DL} pthread)
endif()

set_target_properties(dla_benchmark PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")


if (NOT DISABLE_JIT AND CoreDLA_FOUND)
  file(REAL_PATH ${CoreDLA_DIR}/../lib CoreDLA_LIB EXPAND_TILDE)
  set_target_properties(dla_benchmark PROPERTIES BUILD_RPATH "\$ORIGIN/..:${CoreDLA_LIB}")
else()
  # For libcoreDlaRuntimePlugin.so - typically specified by $COREDLA_ROOT/runtime/plugins.xml
  set_target_properties(dla_benchmark PROPERTIES BUILD_RPATH "\$ORIGIN/..")
endif()

if (NOT EMULATION)
  target_compile_definitions(dla_benchmark PRIVATE DEFAULT_PLUGINS_PATH="$<TARGET_FILE_DIR:coreDlaRuntimePlugin>/plugins.xml")
else()
  target_compile_definitions(dla_benchmark PRIVATE DEFAULT_PLUGINS_PATH="$ENV{COREDLA_ROOT}/lib/plugins.xml")
endif()

# Enable high graph logging by defining its macro
# Change to add_compile_definitions() once we move to cmake >= 3.12
if (DLA_ENABLE_LOGGING)
  target_compile_definitions(dla_benchmark PRIVATE -DENABLE_HG_LOGGING)
endif()

install(TARGETS dla_benchmark RUNTIME DESTINATION "dla/bin" COMPONENT DEMO)

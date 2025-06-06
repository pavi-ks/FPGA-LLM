cmake_minimum_required(VERSION 3.10)

# Use <package>_ROOT variables to help find_package locate packages
if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()

find_package(OpenCV COMPONENTS core highgui imgcodecs imgproc videoio REQUIRED)
find_package(gflags COMPONENTS shared REQUIRED)

add_subdirectory(dla_aot_splitter_plugin)
add_subdirectory(dla_aot_splitter_example)

if (DE10_AGILEX)
  add_library(de10_agilex ALIAS de10_agilex_mmd)
elseif (SYSTEM_CONSOLE_PLATFORM)
  # DO NOTHING
elseif (PAC_A10)
  add_library(dcp_a10_pac ALIAS intel_opae_mmd)
elseif(AGX7_IDK)
  add_library(agx7_i_dk ALIAS intel_opae_mmd)
elseif(AGX7_N6001)
  add_library(agx7_n6001 ALIAS intel_opae_mmd)
endif()

add_executable(dla_aot_splitter ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

target_compile_features(dla_aot_splitter PUBLIC cxx_std_11)

target_sources(dla_aot_splitter PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/dla_aot_splitter.hpp
  $ENV{COREDLA_ROOT}/runtime/dla_benchmark/inputs_filling.cpp #TODO REMOVE and replace with link library
  $ENV{COREDLA_ROOT}/runtime/dla_benchmark/utils.cpp #TODO REMOVE and replace with link library
  $ENV{COREDLA_ROOT}/runtime/common/utils/src/slog.cpp
  $ENV{COREDLA_ROOT}/runtime/common/utils/src/args_helper.cpp
  $ENV{COREDLA_ROOT}/runtime/common/utils/src/latency_metrics.cpp
)

target_include_directories(dla_aot_splitter PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
  $ENV{COREDLA_ROOT}/dla_plugin/inc
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dlia
  $ENV{COREDLA_ROOT}/runtime/dla_benchmark #TODO REMOVE and replace with link library
)

if (WIN32)
  target_include_directories(dla_aot_splitter PRIVATE
    $ENV{COREDLA_ROOT}/compiler/inc # dla_performance_estimator.h
  )
endif()


target_link_libraries(dla_aot_splitter PRIVATE
  openvino::runtime
  openvino_dev_api
  format_reader
  ie_samples_utils
  ${OpenCV_LIBRARIES} # Needed for the directly compiled inputs_filling
  dla_aot_splitter_plugin
  gflags
  dla_util
)

if (NOT WIN32)
  target_link_libraries(dla_aot_splitter PRIVATE
    ${LIB_DL}
    pthread
  )
endif()

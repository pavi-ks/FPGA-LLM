cmake_minimum_required(VERSION 3.10)

add_library(dla_aot_splitter_plugin SHARED)

target_compile_features(dla_aot_splitter_plugin PUBLIC cxx_std_11)

target_compile_definitions(dla_aot_splitter_plugin PUBLIC DISABLE_JIT)

set_target_properties(dla_aot_splitter_plugin PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (WIN32)
  # Fix warning C4273: inconsistent dll linkage
  target_compile_definitions(dla_aot_splitter_plugin PRIVATE XBYAK_NO_OP_NAMES
    IMPLEMENT_INFERENCE_ENGINE_PLUGIN
    $<TARGET_PROPERTY:openvino::runtime,INTERFACE_COMPILE_DEFINITIONS>)
endif()

target_include_directories(dla_aot_splitter_plugin PRIVATE
  $ENV{COREDLA_ROOT}/dla_plugin
  $ENV{COREDLA_ROOT}/dla_plugin/inc
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dlia
  $ENV{COREDLA_ROOT}/inc # dla_dma_constants.svh
  $ENV{COREDLA_ROOT}/runtime/coredla_device/inc # For abstract classes (BatchJob, Device etc.)
  #
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

target_sources(dla_aot_splitter_plugin PRIVATE
##
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dla_async_infer_request.h
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dla_config.hpp
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dla_compiled_model.h
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dla_runtime_log.h
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dlia_infer_request.h
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dlia_plugin.h
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dlia_utils.h
  $ENV{COREDLA_ROOT}/dla_plugin/inc/dla_plugin_config.hpp
##
  $ENV{COREDLA_ROOT}/dla_plugin/src/dla_async_infer_request.cpp
  $ENV{COREDLA_ROOT}/dla_plugin/src/dla_config.cpp
  $ENV{COREDLA_ROOT}/dla_plugin/src/dla_compiled_model.cpp
  $ENV{COREDLA_ROOT}/dla_plugin/src/dlia_infer_request.cpp
  $ENV{COREDLA_ROOT}/dla_plugin/src/dlia_plugin.cpp
  $ENV{COREDLA_ROOT}/dla_plugin/src/dla_plugin_jit_functions.cpp
  $ENV{COREDLA_ROOT}/dla_plugin/src/dlia_utils.cpp
  $ENV{COREDLA_ROOT}/util/src/dla_numeric_utils.cpp
##
  $ENV{COREDLA_ROOT}/runtime/coredla_device/inc/graph_job.h
  $ENV{COREDLA_ROOT}/runtime/coredla_device/inc/batch_job.h
  $ENV{COREDLA_ROOT}/runtime/coredla_device/inc/device.h
##
  ${CMAKE_CURRENT_SOURCE_DIR}/src/raw_graph_job.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/raw_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/raw_batch_job.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/dla_aot_utils.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/raw_graph_job.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/raw_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/raw_batch_job.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/dla_aot_utils.h
  ${CMAKE_CURRENT_SOURCE_DIR}/inc/dla_aot_structs.h
)

if (WIN32)
  target_link_libraries(dla_aot_splitter_plugin
    PRIVATE
##
    dla_op_transformation
    dliaPluginIOTransformations
    openvino::runtime
    openvino_dev_api
    ${TBB_IMPORTED_TARGETS}
)
else()
  target_link_libraries(dla_aot_splitter_plugin
    PRIVATE
##
    pthread
    dla_op_transformation
    dliaPluginIOTransformations
    openvino::runtime
    openvino_dev_api
    ${TBB_IMPORTED_TARGETS}
)
endif()

if (DISABLE_JIT)
  target_include_directories(dla_aot_splitter_plugin PRIVATE
    $ENV{COREDLA_XUTIL_DIR}/compiled_result/inc
  )
  target_link_libraries(dla_aot_splitter_plugin PRIVATE dla_util)
  target_sources(dla_aot_splitter_plugin PRIVATE $ENV{COREDLA_XUTIL_DIR}/compiled_result/src/compiled_result_reader_writer.cpp)

  if (EXISTS $ENV{COREDLA_ROOT}/inc)
    target_include_directories(dla_aot_splitter_plugin PUBLIC $ENV{COREDLA_ROOT}/inc)
  else()
    target_include_directories(dla_aot_splitter_plugin PUBLIC $ENV{COREDLA_ROOT}/build/coredla/dla/inc)
  endif()
else()
  target_link_libraries(dla_aot_splitter_plugin
    PRIVATE
      dla_compiled_result
      archparam
      dla_util
  )
endif()

set_target_properties(dliaPluginIOTransformations PROPERTIES POSITION_INDEPENDENT_CODE ON)

if (WIN32)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plugins_aot_splitter_win.xml ${CMAKE_CURRENT_BINARY_DIR}/plugins_aot_splitter.xml COPYONLY)
else()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plugins_aot_splitter.xml ${CMAKE_CURRENT_BINARY_DIR}/ COPYONLY)
endif()

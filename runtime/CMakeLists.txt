###############################################################################
# Build file defining build targets for the PCIe and HPS example design packages
#
# The runtime can be built in two ways which can be selected with the -disable_jit flag
# using the build_runtime.sh script. Adding the flag will build the AOT (ahead of time)
# runtime ONLY (hence disabling jit/just in time). This build flow should be used if you
# wish to build independently of the DLAC and/or want to port the runtime to your own system.
#
# Building without the -disable_jit flag will build a runtime that is dependent on
# DLAC's libraries which also enables both AOT and JIT flow.
#
# Note: HPS builds default to -disable_jit
##############################################################################

cmake_minimum_required(VERSION 3.10)

# CMake policies
# Use <PackageName>_ROOT env. variable as a prefix
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()
# MSVC runtime library flags are selected by an abstraction.
#if(POLICY CMP0091)
#  cmake_policy(SET CMP0091 NEW)
#endif()
if(POLICY CMP0092)
  # Disable passing /W3 by default on MSVC
  cmake_policy(SET CMP0092 NEW)
endif()
# Honor visibility properties for all target types.
if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

project(coredla_runtime)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm.*|ARM.*|aarch64.*|AARCH64.*)")
  set(ARM ON)
endif()


if (CMAKE_CXX_COMPILER_ID MATCHES MSVC)
  set(CMAKE_CXX_STANDARD 14)
else()
  set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Checking for COREDLA_ROOT being set should be handled by build_runtime.sh
# We just double-check here
if (NOT DEFINED ENV{COREDLA_ROOT})
  message(FATAL_ERROR "COREDLA_ROOT environment variable not set.")
endif()

if (WIN32)
  set(Protobuf_USE_STATIC_LIBS ON)
  list (APPEND CMAKE_PREFIX_PATH $ENV{PROTOBUF_HOME} $ENV{PROTOBUF_LIBS} )
  include(FindProtobuf)
  find_package(Protobuf REQUIRED)
endif()

#
# Adds compiler flags to C / C++ sources
#
macro(dla_add_compiler_flags)
    foreach(flag ${ARGN})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
    endforeach()
endmacro()

get_filename_component(CHK1 $ENV{COREDLA_ROOT}/Makefile ABSOLUTE)
get_filename_component(CHK2 ${CMAKE_CURRENT_SOURCE_DIR}/Makefile ABSOLUTE)
####################################################################
## SDL required compiler flags
####################################################################
# Needed for all builds

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (ARM)
  # Built static library to simplify ED4 files.
  option(BUILD_SHARED_LIBS "Build as a static library" OFF)
else()
  option(BUILD_SHARED_LIBS "Build as a shared library" ON)
endif()

if (WIN32)

  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
      dla_add_compiler_flags(/sdl)
    endif()
    dla_add_compiler_flags("/guard:cf")
    if (ENABLE_INTEGRITYCHECK)
      set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /INTEGRITYCHECK")
    endif()
  endif()

  dla_add_compiler_flags(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
  # CMake adds the following default compiler flags when generating projects for Visual Studio:
  # /DWIN32 /D_WINDOWS /W3 /GR /EHsc
  if (MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  else()
    dla_add_compiler_flags(/EHsc) # no asynchronous structured exception handling
  endif()
  dla_add_compiler_flags(/Gy) # remove unreferenced functions: function level linking
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  # see https://msdn.microsoft.com/en-us/library/fwkeyyhe.aspx for details
  # /ZI = include debug info
  # /Wall = all warnings
  add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:/O2>")
  add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:/ZI>")
  add_compile_options("$<$<CONFIG:RELEASE>:/O2>")
  add_compile_options("$<$<CONFIG:RELEASE>:/D>")
  add_compile_options("$<$<CONFIG:RELEASE>:/NDEBUG>")
  add_compile_options("$<$<CONFIG:DEBUG>:/Od>")
  # buffers security check
  add_compile_options(/GS)

  add_compile_options(/permissive-)

  # Compiler specific flags
  dla_add_compiler_flags(/bigobj)
  dla_add_compiler_flags(/MP)

  # Disable noisy warnings
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4308 /wd4703 /wd4244 /wd4819")
  if (CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4146 /wd4996")
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4146 /wd4996")
    # C4251 needs to have dll-interface to be used by clients of class
    dla_add_compiler_flags(/wd4251)
    # C4275 non dll-interface class used as base for dll-interface class
    dla_add_compiler_flags(/wd4275)
    # Because permissive is set
    dla_add_compiler_flags(/wd5208)

    # inline is not a keyword in visual studios old C version, allow its redefinition
    add_definitions("-D_ALLOW_KEYWORD_MACROS")
  endif()

  # Debug information flags, by default CMake adds /Zi option
  # but provides no way to specify CMAKE_COMPILE_PDB_NAME on root level
  # In order to avoid issues with ninja we are replacing default flag instead of having two of them
  # and observing warning D9025 about flag override
  string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
  string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
  string(REPLACE "/Zi" "/Z7" CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  string(REPLACE "/Zi" "/Z7" CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
else()
  if(DEFINED GLIBC_HEADER)
    # Use a maximum GLIBC of 2.17 (This is not necessary if you build and run on the same machine)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include $ENV{COREDLA_ROOT}/thirdparty/glibc_version_header/force_link_glibc_2.17.h")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -include $ENV{COREDLA_ROOT}/thirdparty/glibc_version_header/force_link_glibc_2.17.h")
  endif()

  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Werror=format-security")

  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Wformat-security")

  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

  set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fPIE")
  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fPIE")

  # Release build only
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")
  if (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fstack-protector-strong")
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -z noexecstack -z relro -z now")

    # These are for 8478-CT158 in the SDL process
    # ( https://sdp-prod.intel.com/bunits/intel/coredla/coredla-ip-20212/tasks/phase/development/8478-CT158/ )
  else()
    set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fstack-protector-all")
  endif()

  # Release build only
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")
  if (GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fstack-protector-strong")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -z noexecstack -z relro -z now")
  else()
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fstack-protector-all")
  endif()

  if (GCC_VERSION VERSION_GREATER 8.0 OR GCC_VERSION VERSION_EQUAL 8.0)
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fstack-clash-protection")
  endif()

  # These are for 8478-CT158 in the SDL process
  # ( https://sdp-prod.intel.com/bunits/intel/coredla/coredla-ip-20212/tasks/phase/development/8478-CT158/ )
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fno-strict-overflow -fno-delete-null-pointer-checks -fwrapv")

  ####################################################################

  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -ggdb3")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -ggdb3")

  #### Sanitizer settings ####
  # Address
  set(CMAKE_C_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  set(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls")

  # Memory
  set(CMAKE_C_FLAGS_MSAN "-O1 -g -fsanitize=memory -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  set(CMAKE_CXX_FLAGS_MSAN "-O1 -g -fsanitize=memory -fno-omit-frame-pointer -fno-optimize-sibling-calls")

  # Thread
  set(CMAKE_C_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls")
  set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls")


  set (CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  # Enable all warnings except unknown-pragmas.  Wunknown-pragmas must be excluded because
  # it is triggered by header file included from OpenCL runtime
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unknown-pragmas")

  # Might be too strict for wide deployment, but easy to disable if it causes problems.
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--enable-new-dtags")
endif()

# DLA specific modifications made to the MMD
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDLA_MMD")

if (${HW_BUILD_PLATFORM} STREQUAL "SYSTEM_CONSOLE")
  set (SYSTEM_CONSOLE_PLATFORM 1)
  set (MMD_DIR_NAME system_console)
  set (MMD_LIB_NAME system_console_mmd)
elseif (${HW_BUILD_PLATFORM} STREQUAL "DE10_AGILEX")
  set (DE10_AGILEX 1)
  set (MMD_DIR_NAME de10_agilex)
  set (MMD_LIB_NAME de10_agilex_mmd)
elseif(${HW_BUILD_PLATFORM} STREQUAL "HPS_PLATFORM")
  set (HPS_PLATFORM 1)
  set (MMD_DIR_NAME hps_platform)
  set (MMD_LIB_NAME hps_platform_mmd)
elseif(${HW_BUILD_PLATFORM} STREQUAL "DCP_A10_PAC")
  set (PAC_A10 1)
  set (MMD_DIR_NAME dcp_a10_pac)
  set (MMD_LIB_NAME intel_opae_mmd)
elseif(${HW_BUILD_PLATFORM} STREQUAL "AGX7_I_DK")
  set (AGX7_IDK 1)
  set (MMD_DIR_NAME agx7_ofs_pcie)
  set (MMD_LIB_NAME intel_opae_mmd)
elseif(${HW_BUILD_PLATFORM} STREQUAL "AGX7_N6001")
  set (AGX7_N6001 1)
  set (MMD_DIR_NAME agx7_ofs_pcie)
  set (MMD_LIB_NAME intel_opae_mmd)
  add_definitions(-DUSE_N6001_BOARD)
else()
  set (EMULATION 1)
endif()

# Set HPS_AGX7 if building for HPS and Agilex 7 is selected
if ("${HW_BUILD_PLATFORM}" STREQUAL "HPS_PLATFORM" AND "${HPS_BUILD_MACHINE}" STREQUAL "agilex7_dk_si_agi027fa")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHPS_AGX7")
endif()

# Set HPS_AGX5 if building for HPS and Agilex 5 is selected
if ("${HW_BUILD_PLATFORM}" STREQUAL "HPS_PLATFORM" AND "${HPS_BUILD_MACHINE}" STREQUAL "agilex5_modular")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHPS_AGX5")
endif()

# Set HPS_PLATFORM if building for HPS
if (HPS_PLATFORM)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHPS_PLATFORM")
endif()

if (NOT HPS_PLATFORM)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_OLD_COREDLA_DEVICE")
endif()

# Flag to disable JIT mode
if (DISABLE_JIT)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDISABLE_JIT")
endif()

# Build the runtime variant of the DLA plugin
add_definitions(-DRUNTIME_DLA_PLUGIN)

##########
if (NOT OpenVINO_DIR)
  Set (OpenVINO_DIR $ENV{OpenVINO_DIR})
endif()

find_package(OpenVINO CONFIG REQUIRED)
#########

if (NOT CoreDLA_DIR)
  set(CoreDLA_DIR $ENV{CoreDLA_DIR})
endif()

if (NOT DISABLE_JIT)
  set(AuditLibstdcxx_LIBSTDCXX_SO_PATHS "${AuditLibstdcxx_LIBSTDCXX_SO_PATHS} ${CMAKE_CURRENT_SOURCE_DIR}" "$ENV{COREDLA_ROOT}/lib" "$ENV{QUARTUS_ROOTDIR}/linux64"
      CACHE STRING "Set candidate paths for libstdc++.so.6 smart resolution")
  find_package(CoreDLA CONFIG REQUIRED)
else()
  add_subdirectory($ENV{COREDLA_ROOT}/util ${CMAKE_CURRENT_BINARY_DIR}/dla_util)
endif()

file(TO_CMAKE_PATH $ENV{COREDLA_ROOT} COREDLA_ROOT)

# Gets a build version of {git branch}-{git hash} as an identifier for the plugin build version. Defaults to "Custom Build"
function(get_build_version output_variable)
  # Get branch name
  execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE result_var_branch
    OUTPUT_VARIABLE branch_name
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  # Get hash
  execute_process(
    COMMAND git rev-parse --short=10 HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE result_var_hash
    OUTPUT_VARIABLE git_hash
    ERROR_QUIET
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT ${result_var_branch} EQUAL 0 OR NOT ${result_var_hash} EQUAL 0)
    set(${output_variable} "Custom build" PARENT_SCOPE)
  else()
    set(${output_variable} "${branch_name}-${git_hash}" PARENT_SCOPE)
  endif()
endfunction()
# Call function to get Git branch name
get_build_version(PLUGIN_BUILD_VERSION)

# Pass PLUGIN_BUILD_VERSION to the source code via a define
add_definitions(-DPLUGIN_BUILD_VERSION="${PLUGIN_BUILD_VERSION}")

if (NOT ARM)
  # ED4 OV is not built with tbb threading
  find_package(TBB REQUIRED tbb)
  if ( NOT DEFINED TBB_IMPORTED_TARGETS)
    set (TBB_IMPORTED_TARGETS TBB::tbb)
  endif()
endif()

SET (INFO 0)
SET (WARNING 1)
SET (ERROR 2)
SET (FATAL 3)

if(DEBUG_RUNTIME)
  add_definitions(-DDEBUG_RUNTIME)
endif(DEBUG_RUNTIME)
if(DEBUG_RUNTIME_MEMORY_TEST)
  add_definitions(-DDEBUG_RUNTIME_MEMORY_TEST)
endif(DEBUG_RUNTIME_MEMORY_TEST)
if(RUNTIME_VERBOSITY)
  add_definitions(-DENABLE_LOGGING)
  add_definitions(-DRUNTIME_VERBOSITY=${${RUNTIME_VERBOSITY}})
endif(RUNTIME_VERBOSITY)

if(RUNTIME_POLLING)
  add_definitions(-DCOREDLA_RUNTIME_POLLING)
endif(RUNTIME_POLLING)

# OpenVINO pre-requisites to build hetero & dla plugin
add_subdirectory(${COREDLA_ROOT}/thirdparty/openvino_dev_api thirdparty/openvino_dev_api)
add_subdirectory($ENV{COREDLA_XUTIL_DIR}/transformations ${CMAKE_CURRENT_BINARY_DIR}/transformations)

# CoreDLA ships the hetero plugin shared library. If AOT only runtime, we re-compile from src and
# do not use the shipped library.
if (DISABLE_JIT)
  add_subdirectory(${COREDLA_ROOT}/thirdparty/pugixml ${CMAKE_CURRENT_BINARY_DIR}/thirdparty/pugixml)
  add_subdirectory(${COREDLA_ROOT}/util/hetero_plugin ${CMAKE_CURRENT_BINARY_DIR}/hetero_plugin)
endif()

# Required for dla_benchmark and demos
add_subdirectory(common)

# Build runtime plugin CPU io transforms
add_subdirectory(${COREDLA_ROOT}/dla_plugin/io_transformations ${CMAKE_BINARY_DIR}/dla_plugin/io_transformations)

# Build Runtime plugin (AGX7/A10/HPS/Emulator)
if(NOT EMULATION)
  file(GLOB SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/src/*.cpp
    ${COREDLA_ROOT}/dla_plugin/src/*.cpp
  )

  if (SYSTEM_CONSOLE_PLATFORM)
    list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/src/mmd_wrapper.cpp)
  endif()

  # We seem to have a partial copy of the compiled_result_reader_writer.cpp inside
  # of plugin/src/dla_compiled_model.cpp. The duplicate code should probably be removed.
  if (DISABLE_JIT)
  list(APPEND SOURCES
    # Only required if building runtime independently of dla
    $ENV{COREDLA_ROOT}/util/src/dla_numeric_utils.cpp
    $ENV{COREDLA_XUTIL_DIR}/compiled_result/src/compiled_result_reader_writer.cpp
  )
  endif()

  file(GLOB HEADERS
    ${COREDLA_ROOT}/dla_plugin/inc/dlia/*.hpp
    ${COREDLA_ROOT}/dla_plugin/inc/*.hpp
    ${COREDLA_ROOT}/dla_plugin/inc/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/inc/*.h
  )
  if (WIN32)
    list(APPEND HEADERS ${COREDLA_ROOT}/fpga/dma/rtl/dla_dma_constants.svh)
  endif()
  add_library(coreDlaRuntimePlugin SHARED ${SOURCES} ${HEADERS})

  if (SYSTEM_CONSOLE_PLATFORM)
    find_package(Boost REQUIRED COMPONENTS filesystem)
    target_sources(coreDlaRuntimePlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/mmd/system_console/mmd_wrapper.cpp)
    add_custom_command(
      TARGET coreDlaRuntimePlugin POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
      ${CMAKE_SOURCE_DIR}/coredla_device/mmd/system_console/system_console_script.tcl
      ${CMAKE_CURRENT_BINARY_DIR}/system_console_script.tcl
    )
    target_link_libraries(coreDlaRuntimePlugin PRIVATE Boost::filesystem)
    # Ensure number of inference request is 1 when using the system-console plugin, and define the system console script path
    target_compile_definitions(coreDlaRuntimePlugin PRIVATE DLA_SYSCON_SOURCE_ROOT=${CMAKE_CURRENT_BINARY_DIR} -DMAX_NUM_INFERENCE_REQUEST=1)
  else()
    target_sources(coreDlaRuntimePlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/src/mmd_wrapper.cpp)
  endif()
endif()

if (WIN32)
  # Fix warning C4273: inconsistent dll linkage
  target_compile_definitions(coreDlaRuntimePlugin PRIVATE XBYAK_NO_OP_NAMES
    IMPLEMENT_INFERENCE_ENGINE_PLUGIN
    $<TARGET_PROPERTY:openvino::runtime,INTERFACE_COMPILE_DEFINITIONS>)
endif()

if (DLA_ALLOW_ENCRYPTION)
  add_definitions(-DDLA_ALLOW_ENCRYPTION)
  find_package(OpenSSL REQUIRED)
  set (CRYPTO_LIB_NAME OpenSSL::Crypto)
endif()

if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lrt" )
endif()

if(NOT EMULATION)
  target_include_directories(coreDlaRuntimePlugin PUBLIC
    ${COREDLA_ROOT}/dla_plugin/inc
    ${COREDLA_ROOT}/dla_plugin/
    ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/inc
    ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/mmd/${MMD_DIR_NAME}/host
    ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/mmd/${MMD_DIR_NAME}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/coredla_device/stream_controller/app
  )
  target_link_libraries(coreDlaRuntimePlugin PUBLIC dla_util)

  if (WIN32)
    target_include_directories(coreDlaRuntimePlugin PUBLIC ${Protobuf_INCLUDE_DIRS})
  endif()

  if (NOT HPS_PLATFORM)
    target_link_libraries(coreDlaRuntimePlugin PUBLIC
      ${CMAKE_DL_LIBS}
      ${TBB_IMPORTED_TARGETS}
      openvino::runtime
      openvino_dev_api
      dliaPluginIOTransformations
      dla_op_transformation
      ${MMD_LIB_NAME}
      ${CRYPTO_LIB_NAME}
    )

    if (NOT DISABLE_JIT)
      target_link_libraries(coreDlaRuntimePlugin PUBLIC
        archparam
        dla_compiler_core
        dla_compiled_result
        lpsolve5525
      )
    endif()
  else()
    target_link_libraries(coreDlaRuntimePlugin PUBLIC
      ${CMAKE_DL_LIBS}
      openvino::runtime
      openvino_dev_api
      dliaPluginIOTransformations
      dla_op_transformation
      ${MMD_LIB_NAME}
    )
  endif()

  # Needed for coredla_device/inc/dla_dma_constants.h to find dla_dma_constants.svh, since
  # the cmake description (find_package(CoreDLA)) does not know about this dependency.
  #
  # Also needed for a variety of .h files in the DISABLE_JIT case, where we do not
  # use the CoreDLA package.
  if (EXISTS ${COREDLA_ROOT}/inc)
    target_include_directories(coreDlaRuntimePlugin PUBLIC ${COREDLA_ROOT}/inc)
  else()
    target_include_directories(coreDlaRuntimePlugin PUBLIC ${COREDLA_ROOT}/build/coredla/dla/inc)
  endif()

  get_property(dirs DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
  foreach(dir ${dirs})
    message(STATUS "dir='${dir}'")
  endforeach()
  add_subdirectory(coredla_device/mmd/${MMD_DIR_NAME})
  # For some reason, (${HW_BUILD_PLATFORM} STREQUAL "DE10_AGILEX") does not work in the line below
  if (DE10_AGILEX)
    add_subdirectory(fpga_jtag_reprogram)
  endif()

  # configure_file replaces ${} variables
  # file(GENERATE replace generator expressions
  # Neither can do both
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plugins.xml ${CMAKE_CURRENT_BINARY_DIR}/plugins_var_subbed.xml)
 # file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/plugins.xml INPUT ${CMAKE_CURRENT_BINARY_DIR}/plugins_var_subbed.xml)

  # This is an ugly hack to keep internal regression tests happy
  if (NOT HPS_PLATFORM)
    if (DEFINED ENV{ARC_JOB_ID})
      if (EXISTS "/p/psg/pac/release/rush_creek/adapt/19.1/367/linux64/sw/deps/lib/libjson-c.so.4")
        target_link_libraries(coreDlaRuntimePlugin PUBLIC "/p/psg/pac/release/rush_creek/adapt/19.1/367/linux64/sw/deps/lib/libjson-c.so.4")
      endif()
    endif()
  endif()
  if (NOT HPS_PLATFORM)
    add_subdirectory(dla_aot_splitter)
  endif()
endif()

add_subdirectory(dla_benchmark)

# Runtime demos are not built by default.
# Pass argument -build_demo to build_runtime.sh to build runtime demos.
if (DEFINED BUILD_DEMO)
  if (DISABLE_JIT)
    message(FATAL_ERROR
      "Error: BUILD_DEMO requires JIT support, but JIT compilation disabled via DISABLE_JIT."
      "       If you did not specify these options, then they may have been cached.  Remove"
      "       ${CMAKE_BINARY_DIR} to clear the cache."
  )
  endif()
  add_subdirectory(classification_sample_async)
  add_subdirectory(object_detection_demo)
  add_subdirectory(segmentation_demo)
endif()

if (HPS_PLATFORM)
  add_subdirectory(streaming/streaming_inference_app)
  add_subdirectory(streaming/image_streaming_app)
endif()

# Runtime install is applicable for Windows only
if (WIN32)
  install(TARGETS coreDlaRuntimePlugin
    RUNTIME DESTINATION "dla/runtime/bin" COMPONENT RUNTIME
    LIBRARY DESTINATION "dla/runtime/lib" COMPONENT RUNTIME
    ARCHIVE DESTINATION "dla/runtime/lib" COMPONENT RUNTIME
  )
endif()

# Add ed0 streaming example tcl script
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/streaming/ed0_streaming_example/system_console_script.tcl
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/streaming/ed0_streaming_example/.)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/streaming/ed0_streaming_example/include/system_console_lib.tcl
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/streaming/ed0_streaming_example/include/.)

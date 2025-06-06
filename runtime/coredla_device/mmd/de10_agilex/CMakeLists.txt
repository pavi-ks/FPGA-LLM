# (C) 2017 Intel Corporation. All rights reserved.
# Your use of Intel Corporation's design tools, logic functions and other
# software and tools, and its AMPP partner logic functions, and any output
# files any of the foregoing (including device programming or simulation
# files), and any associated documentation or information are expressly subject
# to the terms and conditions of the Intel Program License Subscription
# Agreement, Intel MegaCore Function License Agreement, or other applicable
# license agreement, including, without limitation, that your use is for the
# sole purpose of programming logic devices manufactured by Intel and sold by
# Intel or its authorized distributors.  Please refer to the applicable
# agreement for further details.

cmake_minimum_required(VERSION 2.8.12)
project(mmd)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# DLA specific modifications made to the MMD
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDLA_MMD")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACL_MAX_DEVICE=128")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DOPTION3=1 -DACL_USE_DMA=1")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACL_HAS_STDLIB_STDIO")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACL_OPENCL_HOST_BIT=64 -DACL_TARGET_BIT=64")

# Select PCIE Gen3 x16
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGEN3_x16")

if (WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DAOCL_MMD_CALL=__declspec(dllexport)")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACL_COMPILER_IS_MSVC=1 -DACL_HOST_RUNTIME_IS_STATIC=1")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACL_OPENCL_HOST_SYS=windows -DACL_TARGET_SYS=windows -DWINDOWS")
endif()

# from the opencl makefile
if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DKERNEL_64BIT -O3 -DACL_COMPILER_IS_MSVC=0 -DACL_HOST_RUNTIME_IS_STATIC=0")
  # No _GLIBCXX_USE_CXX11_ABI=0 since OpenVINO 24.4 will force _GLIBCXX_USE_CXX11_ABI=1
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-unknown-pragmas -fstack-protector -Wformat -Wformat-security -O2")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DACL_OPENCL_HOST_SYS=linux -DACL_TARGET_SYS=linux -DLINUX")
  # Release build only
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -D_FORTIFY_SOURCE=2")
endif()

enable_language(C ASM)

set(ASM_OPTIONS "-x assembler-with-cpp")
if(${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  set(ASM_OPTIONS "${ASM_OPTIONS} -no-integrated-as")
endif()

set(CMAKE_ASM_FLAGS "${CFLAGS} ${ASM_OPTIONS}")

set(MMD_SRC
   ./host/acl_pcie_config.cpp
   ./host/acl_pcie.cpp
   ./host/acl_pcie_debug.cpp
   ./host/acl_pcie_device.cpp
   ./host/acl_pcie_dma_linux.cpp
   ./host/acl_pcie_dma_windows.cpp
   ./host/acl_pcie_hostch.cpp
   ./host/acl_pcie_mm_io.cpp
   ./host/acl_pcie_timer.cpp
)

add_library(de10_agilex_mmd SHARED ${MMD_SRC})

target_include_directories(de10_agilex_mmd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if (WIN32)
  # Terrasic production BSP Linux kernel space driver header files
  set(TERASIC_KERNEL_HEADER_DIR $ENV{AOCL_BOARD_PACKAGE_ROOT}/linux64/driver)
  set(TERASIC_KERNEL_HEADER_FILES
    fpga_cmd_guids.h
    hw_host_channel.h
    hw_pcie_constants.h
    hw_pcie_dma.h
  )
  if (EXISTS ${TERASIC_KERNEL_HEADER_DIR})
    foreach(header ${TERASIC_KERNEL_HEADER_FILES})
      if (EXISTS ${TERASIC_KERNEL_HEADER_DIR}/${header})
        file(COPY ${TERASIC_KERNEL_HEADER_DIR}/${header} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/include)
      else()
        message(WARNING "Header file ${header} does not exist in ${TERASIC_KERNEL_HEADER_DIR}")
      endif()
    endforeach()
  else()
    message(FATAL_ERROR "Source directory ${TERASIC_KERNEL_HEADER_DIR} does not exist.")
  endif()

  set(HW_PCI_DMA_H ${CMAKE_CURRENT_SOURCE_DIR}/include/hw_pcie_dma.h)
  file(READ ${HW_PCI_DMA_H} HW_PCI_DMA_H_CONTENT)
  # Remove any end-of-line whitespace from the file content (spaces and tabs)
  string(REGEX REPLACE "[ \t]+(\r?\n)" "\\1" HW_PCI_DMA_H_CONTENT "${HW_PCI_DMA_H_CONTENT}")
  set(OLD_CODE_BLOCK
"PACK(
struct DMA_DESC_ENTRY {
    UINT32 src_addr_ldw;
    UINT32 src_addr_udw;
    UINT32 dest_addr_ldw;
    UINT32 dest_addr_udw;
    UINT32 ctl_dma_len;
    UINT32 reserved[3];
});")
  set(NEW_CODE_BLOCK
"#if defined(GEN3_x8)
PACK(
struct DMA_DESC_ENTRY {
    UINT32 src_addr_ldw;
    UINT32 src_addr_udw;
    UINT32 dest_addr_ldw;
    UINT32 dest_addr_udw;
    UINT32 ctl_dma_len;
    UINT32 reserved[3];
});
#elif defined(GEN3_x16)
PACK(
struct DMA_DESC_ENTRY {
    UINT64 src_addr;
    UINT64 dst_addr;
    UINT32 ctrl;
    UINT32 reserved[3];
});
#endif")
  string(REPLACE "${OLD_CODE_BLOCK}" "${NEW_CODE_BLOCK}" HW_PCI_DMA_H_CONTENT "${HW_PCI_DMA_H_CONTENT}")
  file(WRITE ${HW_PCI_DMA_H} "${HW_PCI_DMA_H_CONTENT}")

  set_target_properties(de10_agilex_mmd PROPERTIES LINK_FLAGS "-subsystem:console -nologo -fixed:no -incremental:no -opt:noref -ignore:4089 /NXCOMPAT /DYNAMICBASE")

  find_library(ACL_CHECK_SYS_CMD_LIB
    acl_check_sys_cmd
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib/win64)
  find_library(FPGA_LIB
    FpgaLib
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/lib/win64)

  target_link_libraries(de10_agilex_mmd ${ACL_CHECK_SYS_CMD_LIB} ${FPGA_LIB})
else()
  target_link_libraries(de10_agilex_mmd)
endif()

install(TARGETS de10_agilex_mmd
  RUNTIME DESTINATION "dla/runtime/bin" COMPONENT de10_agilex_mmd
  LIBRARY DESTINATION "dla/runtime/lib" COMPONENT de10_agilex_mmd
  ARCHIVE DESTINATION "dla/runtime/lib" COMPONENT de10_agilex_mmd
)

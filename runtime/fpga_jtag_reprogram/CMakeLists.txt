cmake_minimum_required(VERSION 3.10)

if (CMAKE_CXX_COMPILER_ID MATCHES MSVC)
  set(CMAKE_CXX_STANDARD 14)
else()
  set(CMAKE_CXX_STANDARD 11)
endif()

file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(fpga_jtag_reprogram ${SOURCES})

if (NOT WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lrt" )
endif()
target_link_libraries(fpga_jtag_reprogram PRIVATE  ${CMAKE_DL_LIBS} de10_agilex_mmd)

install(TARGETS fpga_jtag_reprogram RUNTIME DESTINATION "dla/bin" COMPONENT COREDLA)

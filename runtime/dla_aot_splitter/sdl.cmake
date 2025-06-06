
####################################################################
## SDL required compiler flags
####################################################################
# Needed for all builds
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wformat -Wformat-security -Werror=format-security")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat -Wformat-security")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

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

set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC -D_FORTIFY_SOURCE=2")
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

# Make warnings errors to avoid having them in SDL report
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")

# Should cleanup the signed and unsigned compares then remove this exception
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=sign-compare -Wno-error=unused-function -Wno-error=switch -Wno-error=unused-variable -Wno-error=unused-value -Wno-error=unused-but-set-variable -Wno-error=undef -Wno-error=return-type -Wno-error=reorder")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--enable-new-dtags")

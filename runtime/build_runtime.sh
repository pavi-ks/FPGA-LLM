#!/bin/bash

#=============================================================================
# Script to build runtime plugin
#=============================================================================
GDB=0

usage()
{
    echo "Build runtime plugin"
    echo "Usage: ./build_runtime.sh [OPTIONS]"
    echo "  -h                                      Show this help"
    echo "  -cmake_debug                            Flag to compile in debug mode"
    echo "  -verbosity=INFO|WARNING|ERROR|FATAL     Enable logging at desired verbosity"
    echo "  -build_dir=<path>                       Location of the runtime build"
    echo "  -disable_jit                            Disable JIT execution mode - this removes dependencies on precompiled DLA libraries"
    echo "  -build_demo                             Build runtime demos"
    echo "  -target_de10_agilex                     Target the DE10 Agilex board"
    echo "  -target_agx7_i_dk                       Target the Agilex 7 I-Series board"
    echo "  -target_agx7_n6001                      Target the Agilex N6001 board"
    echo "  -target_emulation                       Target the software emulation (aka emulator) build"
    echo "  -target_system_console                  Target a device that communicates with the host via system-console"
    echo "  -hps_platform                           Target HPS-based hardware."
    echo "                                          This option should be used only via the create_hps_image.sh script"
    echo "  -hps_machine=<machine>                  Target a specific machine. Used with -hps_platform. Options: arria10 (Arria 10 SoC),"
    echo "                                          agilex7_dk_si_agi027fa (Agilex 7 SoC), agilex5_modular (Agilex 5 SoC) [if not specified,"
    echo "                                          default is Arria 10]"
    echo "  -aot_splitter_example                   Build the aot splitter example"
}

hidden_usage()
{
    usage
    echo "  -coredla_dir_cmake=<path>               Intended for regtest which packages coredla and runtime separately"
    echo "  -coredla_dir_lib=<path>                 Intended for regtest which packages coredla and runtime separately"
    echo "  -encryption=<0|1>                       Without OpenSSL, can still build runtime without encryption support"
    echo "  -no_make                                Skip final make command for Klocwork"
    echo "  -polling                                Use polling instead of interrupts"
    echo "  -target_a10_pac                         Target the Arria 10 PAC [EOL 2024.1 Release]"
    echo "  -run_tests                              Runs short build tests. For Altera internal usage only."
    echo "  -glibc_header                           Force inclusion of glibc pinning header. "
    echo "                                          Use with caution if runtime binaries must be distributed to machines with differeing GLIBC versions"
}

OPT_RUNTIME_POLLING=false
OPT_DISABLE_JIT=false
OPT_BUILD_DEMO=false
OPT_RUN_TESTS=false
OPT_GLIBC_HEADER=false
RUNTIME_VERBOSITY="-DRUNTIME_VERBOSITY=0"

TARGET=""

#Terrasic production BSP kernel space driver header files
TERASIC_KERNEL_HEADER_FILES="hw_host_channel.h hw_pcie_constants.h pcie_linux_driver_exports.h"

for i in "$@"; do
    case $i in
        -h | --help )                                   usage
                                                        exit
                                                        ;;
        -cmake_debug | --cmake_debug )                  GDB=1
                                                        ;;
        -verbosity=* | --verbosity=* )                  RUNTIME_VERBOSITY="-DRUNTIME_VERBOSITY=${i#*=}"
                                                        shift # pass argument=value
                                                        ;;
        -build_dir=* | --build_dir=* )                  BUILD_DIR_USER="${i#*=}"
                                                        shift
                                                        ;;
        -disable_jit | --disable_jit )                  OPT_DISABLE_JIT=true
                                                        ;;
        -build_demo | --build_demo )                    OPT_BUILD_DEMO=true
                                                        ;;
        -target_de10_agilex | --target_de10_agilex )    PLATFORM_NAME="Terasic DE 10"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=DE10_AGILEX"
                                                        ;;
        -target_a10_pac | --target_a10_pac )            PLATFORM_NAME="PAC A10"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=DCP_A10_PAC"
                                                        ;;
        -target_agx7_i_dk | --target_agx7_i_dk )        PLATFORM_NAME="AGX7 ISERIES DK"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=AGX7_I_DK"
                                                        ;;
        -target_agx7_n6001 | --target_agx7_n6001 )      PLATFORM_NAME="AGX7 N6001"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=AGX7_N6001"
                                                        ;;
        -target_emulation | --target_emulation )        PLATFORM_NAME="EMULATION"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=EMULATION"
                                                        ;;
        -target_system_console | --target_system_console ) PLATFORM_NAME="SYSTEM CONSOLE"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=SYSTEM_CONSOLE"
                                                        OPT_RUNTIME_POLLING=true
                                                        ;;
        # If HPS then we disable the JIT.
        -hps_platform | --hps_platform )                PLATFORM_NAME="ARM Soc FPGA Platform"
                                                        BUILD_PLATFORM="-DHW_BUILD_PLATFORM=HPS_PLATFORM"
                                                        OPT_DISABLE_JIT=true
                                                        HPS_PLATFORM_BUILD=1
                                                        OPT_RUNTIME_POLLING=true
                                                        HPS_BUILD_MACHINE="-DHPS_BUILD_MACHINE=arria10"
                                                        ;;
        # Specify the HPS machine. Default is Arria 10.
        -hps_machine=* | --hps_machine=* )              if [ -z ${HPS_PLATFORM_BUILD} ]; then
                                                            echo "Error: -hps_machine can only be specified with -hps_platform"
                                                            exit 1
                                                        fi
                                                        HPS_BUILD_MACHINE="-DHPS_BUILD_MACHINE=${i#*=}"
                                                        ;;
        -aot_splitter_example | --aot_splitter_example ) TARGET="dla_aot_splitter_example"
                                                         ;;
        -ed3_streaming_example | --ed3_streaming_example ) TARGET="ed3_streaming_example"
                                                         ;;
        # all options below are hidden features and therefore not listed in usage()
        -hidden_help | --hidden_help )                  hidden_usage
                                                        exit
                                                        ;;
        -coredla_dir_cmake=* | --coredla_dir_cmake=* )  COREDLA_DIR_USER_CMAKE="${i#*=}"
                                                        shift
                                                        ;;
        -coredla_dir_lib=* | --coredla_dir_lib=* )      COREDLA_DIR_USER_LIB="${i#*=}"
                                                        shift
                                                        ;;
        -encryption=* | --encryption=* )                ENCRYPTION_USER="${i#*=}"
                                                        shift
                                                        ;;
        -no_make | --no_make )                          SKIP_MAKE=1
                                                        ;;
        -polling | --polling )                          OPT_RUNTIME_POLLING=true
                                                        ;;
        -run_tests | --run_tests )                      OPT_RUN_TESTS=true
                                                        ;;
        -glibc_header | --glibc_header )                OPT_GLIBC_HEADER=true
                                                        ;;
        * )                                             echo "Error: Unrecognised argument: $i"
                                                        usage
                                                        exit 1
    esac
    shift
done

if [[ -z "${PLATFORM_NAME}"  ]]; then
    echo "Error: Please specify which platform to build the runtime for. Run ./build_runtime.sh -h to see usage"
    exit 1
fi

# Currently runtime demos do not work with the AOT flow. Throw an error to remind the user.
if $OPT_DISABLE_JIT && $OPT_BUILD_DEMO; then
    echo "Error: Cannot build runtime demos with JIT disabled."
    exit 1
fi

# set to 0 to remove OpenSSL dependency from customer flow
# IMPORTANT (for Intel release manager): this must be consistent with ALLOW_ENCRYPTION in ${COREDLA_ROOT}/Makefile
ALLOW_ENCRYPTION=0
if [ ! -z "$ENCRYPTION_USER" ]; then
    ALLOW_ENCRYPTION=${ENCRYPTION_USER}
fi
if [ "$ALLOW_ENCRYPTION" == "1" ]; then
    DLA_ALLOW_ENCRYPTION="-DDLA_ALLOW_ENCRYPTION=1"
fi

if [[ -z "${COREDLA_ROOT}" ]]; then
    echo "Error: COREDLA_ROOT environment variable not set. Run init_env.sh script first."
    exit 1
fi

# if CoreDLA Config Directory is not under root check under build directory
COREDLA_DIR_CMAKE=${COREDLA_ROOT}/cmake
COREDLA_DIR_LIB=${COREDLA_ROOT}/lib

echo ${COREDLA_DIR_USER_CMAKE}

# Only need to check if cmake exists since COREDLA_ROOT/cmake and COREDLA_ROOT/lib are in same paths
if [[ ! -e "${COREDLA_DIR_CMAKE}/CoreDLATargets.cmake" && -d "${COREDLA_ROOT}/build/coredla/dla/cmake" ]]; then
    COREDLA_DIR_CMAKE=${COREDLA_ROOT}/build/coredla/dla/cmake
    COREDLA_DIR_LIB=${COREDLA_ROOT}/build/coredla/dla/lib
fi
if [ ! -z "$COREDLA_DIR_USER_CMAKE" ]; then
    COREDLA_DIR_CMAKE=${COREDLA_DIR_USER_CMAKE}
    COREDLA_DIR_LIB=${COREDLA_DIR_USER_LIB}
fi
if [ ! -d "$COREDLA_DIR_CMAKE" ]; then
   # This error should not be possible in a correctly deployed build.  It should
   # only happen in a developer environment.
   echo "Error: $COREDLA_DIR_CMAKE not found.  Did you remember to do: cd \$COREDLA_ROOT && make"
   exit 1
fi

# A deployed build has $COREDLA_ROOT/util/compiled_result/, $COREDLA_ROOT/util/transformations/,
# whereas an Intel-internal developer build has these as just $COREDLA_ROOT/compiled_result/, etc.
# Would prefer to syncrhonize these.  They could be found within cmake by ${CoreDLA_DIR}/../util/,
# but that would mean developer changes to compiled_result/ and transformations/ need a `make`
# before they are visible to the runtime build.
if [ -d "$COREDLA_ROOT"/util/compiled_result ]; then
   COREDLA_XUTIL_DIR="$COREDLA_ROOT"/util
else
   COREDLA_XUTIL_DIR="$COREDLA_ROOT"
fi
export COREDLA_XUTIL_DIR

if [ ! -z "${PLATFORM_NAME}" ]; then
    echo "Building runtime for ${PLATFORM_NAME}"
fi

if $OPT_BUILD_DEMO; then
    echo "Runtime demos will be built."
    if [ "$GDB" == "1" ]; then
        echo "To test the runtime demo performance, please use the release build instead."
    fi
fi

SCRIPT_DIR=$(cd "$(dirname $0)" >/dev/null 2>&1 && pwd)
RUNTIME_ROOT_DIR=$(cd "${SCRIPT_DIR}" >/dev/null 2>&1 && pwd)

cd ${RUNTIME_ROOT_DIR}

BUILD_TYPE=Release

if [ "$GDB" == "1" ]; then
    echo "Building in debug mode"
    BUILD_TYPE=Debug
fi

BUILD_DIR=build_${BUILD_TYPE}
if [ ! -z "$BUILD_DIR_USER" ]; then
    BUILD_DIR=${BUILD_DIR_USER}
fi


if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p ${BUILD_DIR}
fi

# Checking the type of build
if [ "${PLATFORM_NAME}" = "Terasic DE 10" ]; then
    if [ ! -z "${AOCL_BOARD_PACKAGE_ROOT}" ]; then
        echo "Copying necessary header files from Terasic Production BSP"
        for i in $TERASIC_KERNEL_HEADER_FILES; do
            if ! cp "${AOCL_BOARD_PACKAGE_ROOT}/linux64/driver/${i}" "${RUNTIME_ROOT_DIR}/coredla_device/mmd/de10_agilex/host/"; then
                echo "Error: Unable to copy ${i} from ${AOCL_BOARD_PACKAGE_ROOT}/linux64/driver"
                exit 1
            fi
        done
    else
        echo "Error: Environment variable AOCL_BOARD_PACKAGE_ROOT must be set to the Terasic BSP path."
        exit 1
    fi
fi

if [ "${PLATFORM_NAME}" = "PAC A10" ]; then
    if [[ -z "${OPAE_SDK_ROOT}" ]]; then
        echo "Error: OPAE_SDK_ROOT environment variable not set. Run OPAE setup script before."
        echo "       If OPAE has been installed into the default location, use: export OPAE_SDK_ROOT=/usr"
        exit 1
    fi
    # Some quick checks that OPAE exists where we expect it.  Note that
    # coredla_device/mmd/dcp_a10_pac/cmake/modules/FindOPAE.cmake has some search locations
    # for OPAE hardcoded - so in reality, we will find it in /usr/ even if $OPAE_SDK_ROOT
    # points to the wrong directory; but prefer to enforce a valid $OPAE_SDK_ROOT here.
    if [ ! -f "$OPAE_SDK_ROOT/include/opae/fpga.h" -o \
            ! \( -f "$OPAE_SDK_ROOT/lib/libopae-c.so" -o -f "$OPAE_SDK_ROOT/lib64/libopae-c.so" \) ]
    then
        echo "Error: OPAE not found at location specified by OPAE_SDK_ROOT."
        exit 1
    fi

    OPAE_DIR="-DLIBOPAE-C_ROOT=${OPAE_SDK_ROOT}/"
fi

if [ "${PLATFORM_NAME}" = "AGX7 ISERIES DK" ] || [ "${PLATFORM_NAME}" = "AGX7 N6001" ]; then
    if [[ -z "${OPAE_SDK_ROOT}" ]]; then
        echo "Error: OPAE_SDK_ROOT environment variable not set. Run OPAE setup script before."
        echo "       If OPAE has been installed into the default location, use: export OPAE_SDK_ROOT=/usr"
        exit 1
    fi
    # Some quick checks that OPAE exists where we expect it.  Note that
    # coredla_device/mmd/agx7_ofs_pcie/cmake/modules/FindOPAE.cmake has some search locations
    # for OPAE hardcoded - so in reality, we will find it in /usr/ even if $OPAE_SDK_ROOT
    # points to the wrong directory; but prefer to enforce a valid $OPAE_SDK_ROOT here.
    if [ ! -f "$OPAE_SDK_ROOT/include/opae/fpga.h" -o \
            ! \( -f "$OPAE_SDK_ROOT/lib/libopae-c.so" -o -f "$OPAE_SDK_ROOT/lib64/libopae-c.so" \) ]
    then
        echo "Error: OPAE not found at location specified by OPAE_SDK_ROOT."
        exit 1
    fi

    OPAE_DIR="-DLIBOPAE-C_ROOT=${OPAE_SDK_ROOT}/"
fi

if [ "${PLATFORM_NAME}" = "EMULATION" ]; then
    if [ -z "${COREDLA_DIR_LIB}/libdla_emulator.so" ]; then
         # This should not happen in a correctly deployed build
        echo "The software emulator shared library libdla_emulator.so does not exist in ${COREDLA_DIR_LIB}"
        exit 1
    fi
fi

if $OPT_RUNTIME_POLLING; then
    echo "Warning: using polling instead of interrupts"
fi

if $OPT_DISABLE_JIT; then
    echo "Building without just-in-time (JIT) execution functionality"
fi

# We must specify a default for $RUNTIME_POLLING so that cmake does a rebuild if
# the polling option changes.
RUNTIME_POLLING="-DRUNTIME_POLLING=0";
$OPT_RUNTIME_POLLING && RUNTIME_POLLING="-DRUNTIME_POLLING=1"

# We must specify a default for $DSIABLE_JIT sot hat cmake does a rebuild if the
# option changes.
DISABLE_JIT="-DDISABLE_JIT=0";
$OPT_DISABLE_JIT && DISABLE_JIT="-DDISABLE_JIT=1"

# We use a default of "" for BUILD_DEMO.  This means that cmake will not force a rebuild
# if the -build_demo option is specified on a first build and then not specified on a second
# build (unless something else forces a rebuild, of course).
#
BUILD_DEMO=""
$OPT_BUILD_DEMO && BUILD_DEMO="-DBUILD_DEMO=1"

GLIBC_HEADER=""
$OPT_GLIBC_HEADER && GLIBC_HEADER="-DGLIBC_HEADER=1"

# On Ubuntu18 devices demos may break with:
# "Cannot load library ... libcoreDLARuntimePlugin.so ... undefined symbol: dla_mmd_ddr_write"
# This is caused if /opt/intelFPGA_pro/quartus_19.2.0b57/hld/host/linux64/lib is in LD_LIBRARY_PATH
# The known fix is to simply remove it
os_release=$(lsb_release -rs)
conflicting_dir="intelFPGA_pro/quartus_19.2.0b57/hld/host/linux64/lib"
if [[ "$os_release" == "18."* && "$OPT_BUILD_DEMO" == true && ":$LD_LIBRARY_PATH:" == *"$conflicting_dir:"* ]]; then
    echo -e "\e[91mError: Ubuntu18 runtime demo build detected. The demos may break with $conflicting_dir in the LD_LIBRARY_PATH. Please remove and recompile.\e[0m"
    exit 1
fi

if [ -z ${HPS_PLATFORM_BUILD} ]; then
    if [ -z "${COREDLA_GCC}" ]; then
        COREDLA_GCC=gcc
        COREDLA_GXX=g++
    else
        COREDLA_GXX=$(dirname $COREDLA_GCC)/g++
    fi
    # When this happens (ie: differing g++ and gcc versions), the resulting link errors can be
    # rather confusing.  Perhaps testing for this is overkill?  We used to allow an environment
    # variable override of CXX, which is what made it easier to induce the version mismatch.
    CC_VERSION=$(${COREDLA_GCC} --version | head -1 | awk '{print $NF}')
    CXX_VERSION=$(${COREDLA_GXX} --version | head -1 | awk '{print $NF}')
    if [ "$CC_VERSION" = "" -o "$CC_VERSION" != "$CXX_VERSION" ]; then
        echo "Error: ${COREDLA_GCC} version is \"$CC_VERSION\" but ${COREDLA_GXX} version is \"$CXX_VERSION\""
        echo "       Both compilers must have the same version number."
        exit 1
    fi

    set -x

    cd ${BUILD_DIR} || exit 1
    # Runtime demos will not be built by default. Use the -build_demo flag to build them.
    CC=${COREDLA_GCC} CXX=${COREDLA_GXX} cmake ${RUNTIME_VERBOSITY} ${RUNTIME_POLLING} ${BUILD_PLATFORM} ${OPAE_DIR} ${DLA_ALLOW_ENCRYPTION} -DCoreDLA_DIR=${COREDLA_DIR_CMAKE} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${RUNTIME_ROOT_DIR} ${DISABLE_JIT} ${BUILD_DEMO} ${GLIBC_HEADER}

    cmake_exit_code=$?
    set +x

else
    # Setup the Yocto Toolchain
    ${RUNTIME_ROOT_DIR}/scripts/hps/setup_toolchain.sh poky*.sh
    if [ $? != 0 ]; then
        echo -e "\nNote: Directly calling build_runtime.sh --hps_platform is for internal only. "\
                "If you are building runtime for ED4, use ${RUNTIME_ROOT_DIR}/create_hps_image.sh instead.\n"
        exit 1
    fi
    TOOLCHAIN_FILE=${RUNTIME_ROOT_DIR}/embedded_arm_sdk/cmake/embedded.arm.cmake

    HPS_PACKAGES_DIR=`pwd`/hps_packages
    HPS_INSTALL_PACKAGES=${HPS_PACKAGES_DIR}/armcpu_package

    export INTEL_OPENVINO_DIR=${HPS_INSTALL_PACKAGES}
    # Check that the Local OPENVINO build has been done
    if [ ! -e ${INTEL_OPENVINO_DIR} ]; then
        echo "Error: Pre-built openvino package not found."
        echo "     : Run ./build_hpspackages.sh -sb"
        echo -e "\nNote: Directly calling build_runtime.sh --hps_platform is for internal only. "\
                "If you are building runtime for ED4, use ${RUNTIME_ROOT_DIR}/create_hps_image.sh instead.\n"
        exit 1
    fi
    # in OpenVINO 2022.3, setupvars.sh sits in ${INTEL_OPENVINO_DIR}, not in S{INTEL_OPENVINO_DIR}/bin
    source ${INTEL_OPENVINO_DIR}/setupvars.sh

    unset gflags_ROOT
    export gflags_ROOT=${HPS_INSTALL_PACKAGES}/gflags
    CMAKE_OPTIONS=-DCMAKE_STAGING_PREFIX=${HPS_INSTALL_PACKAGES}

    set -x
    cd ${BUILD_DIR} || exit 1
    cmake -DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE} ${RUNTIME_VERBOSITY} ${RUNTIME_POLLING} ${BUILD_PLATFORM} ${HPS_BUILD_MACHINE} ${OPAE_DIR} ${DLA_ALLOW_ENCRYPTION} -DCoreDLA_DIR=${COREDLA_DIR_CMAKE} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${DISABLE_JIT} ${BUILD_DEMO} ${CMAKE_OPTIONS} ${RUNTIME_ROOT_DIR}  ${GLIBC_HEADER}

    cmake_exit_code=$?
    set +x
fi

if [ $cmake_exit_code != 0 ]; then
    echo "Error: cmake failed"
    exit 1
fi

function version { echo "$@" | awk -F. '{ printf("%d%03d%03d%03d\n", $1,$2,$3,$4); }'; }
cmake_dot_version=$(cmake --version | grep 'cmake version' | awk '{print $3}')
CMAKE_PARALLEL=
if [ $(version ${cmake_dot_version} ) -ge $(version "3.12.0") ]; then
  # Check total_mem so that we are consistent between runs (even through free_mem is arguably
  # more relevant).
  total_mem=`free -g | grep Mem | awk '{print $2}'`
  if [ "$total_mem" -gt 48 ]; then
    CMAKE_PARALLEL="--parallel"
  fi
fi

# Check if we should skip the make process
if [ "$SKIP_MAKE" != "1" ]; then
    if [ "${TARGET}" != "" ]; then
        set -x
        cmake --build . --target "${TARGET}" ${CMAKE_PARALLEL}
        make_result=$?
        set +x
    else
        set -x
        cmake --build . ${CMAKE_PARALLEL}
        make_result=$?
        set +x
    fi

    # If the build failed, exit with the make result
    if [ $make_result -ne 0 ]; then
        exit $make_result
    fi
fi

# Check if tests should be run based on OPT_RUN_TESTS variable
if [ "$OPT_RUN_TESTS" = "true" ]; then
    if [[ -n "$GITHUB_REPOSITORY_OWNER" ]]; then
        # Runs in GitHub
        LINKER_TEST_SCRIPT="$GITHUB_WORKSPACE/runtime/scripts/internal/linker_test.sh"
    else
        # Runs locally
        LINKER_TEST_SCRIPT="$RUNTIME_ROOT_DIR/scripts/internal/linker_test.sh"
    fi
    if [ -f "$LINKER_TEST_SCRIPT" ]; then
        # Notify that the build was successful and tests are starting
        echo -e "\033[1;33mBuild successful. Running linker test...\033[0m"
        if ! "$LINKER_TEST_SCRIPT" "$BUILD_DIR" "$PLATFORM_NAME" "$OPT_DISABLE_JIT"; then
            echo "Error: Linker test script failed with a non-zero return code." >&2
            exit 1
        fi
    else
        echo "Error: Tests not found."
        exit 1
    fi
fi

# If we reach this point, the build and any tests were successful
exit 0

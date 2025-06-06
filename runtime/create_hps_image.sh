#! /bin/bash
set -o errexit

#
# This script is a starter script for building an ED4 SD Card image on the HPS platform.
# This script wraps the following steps:
# 1. Build bitstreams (S2M) if specified
# 2. Build Yocto SD card image (.wic) to obtain the toolchain SDK or use prebuilt .wic.
# 3. Use the SDK to cross-build HPS packages and CoreDLA runtime
# 4. Update the .wic image with CoreDLA executable, libraries, and FPGA bitstreams

if [[ -z "${COREDLA_ROOT}" ]]; then
    echo "Error: COREDLA_ROOT environment variable not set. Run init_env.sh script first."
    exit 1
fi


SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# runtime
RUNTIME_DIR="${SCRIPT_PATH}"  # this script should always be located in runtime
RUNTIME_BUILD_TYPE="Release"  # Release or Debug. Release will build OpenVINO and DLA in debug mode
RUNTIME_BUILD_DIR="${RUNTIME_DIR}/build_${RUNTIME_BUILD_TYPE}"
BUILD_RUNTIME_SCRIPT="${RUNTIME_DIR}/build_runtime.sh"

# Yocto
ED4_DIR="${COREDLA_ROOT}/hps/ed4"
ED4_SCRIPT_DIR="${ED4_DIR}/scripts"
UPDATE_SDCARD_SCRIPT="${ED4_SCRIPT_DIR}/update_sd_card.sh"
USING_PREBUILT_YOCTO=false
UPDATE_SDCARD=false
YOCTO_DIR="${ED4_DIR}/yocto"
YOCTO_BUILD_SCRIPT="${YOCTO_DIR}/run-build.sh"
YOCTO_BUILD_DIR="${RUNTIME_DIR}/build_Yocto"
MACHINE="arria10"  # default device family

# HPS packages
BUILD_HPSPACKAGES_SCRIPT="${RUNTIME_DIR}/build_hpspackages.sh"
HPSPACKAGES_BUILD_DIR="${RUNTIME_DIR}/hps_packages"

# Build bitstreams
BUILD_BITSTREAM=false
BUILD_BITSTREAM_SCRIPT="${COREDLA_ROOT}/bin/dla_build_example_design.py"

# Experimental
BUILD_EXPERIMENTAL_SCRIPT=${RUNTIME_DIR}/build_experimental.sh
BUILD_EXPERIMENT_FEATURE=false

#################################################################
function usage
{
    echo -e "\nThis script is a starter script for building an ED4 SD Card image on the HPS platform."\
            "This script wraps the following steps:"\
            "\n\t1. Build an S2M bitstream of choice for Arm-based SoC (if specified)"\
            "\n\t2. Build a Yocto SD card image (.wic) and obtain the toolchain SDK."\
            "\n\t3. Use the SDK from step 2 to cross-compile dependency packages and CoreDLA runtime"\
            "\n\t4. Update the .wic image with CoreDLA runtime, libraries, and FPGA bitstream"\
            "\n"
    echo "create_hps_image.sh -o <PATH> [-f <PATH>] [-y] [-u] [-b] [-h] [-a <PATH>] [-m <FPGA Machine>]"
    echo ""
    echo "Options:"
    echo -e "  -h Display usage"
    echo -e "  -c Clean the runtime directory back to its default state"
    echo -e "  -o (Required) Path to the output directory to save the updated SD card image."
    echo -e "  -u (Optional) Specify this to update the SD card .wic image at the end.  If this option is set, the DLA runtime"
    echo -e "     and the FPGA bitstream specified by -f will be written to .wic image at the end.  This option is helpful"
    echo -e "     if a user only wants to build the DLA runtime without a working bitstream. In this case, you can skip"
    echo -e "     setting -u and -f."
    echo -e "  -b (Optional) Whether to build a bitstream (S2M) using the arch file specified by -a."
    echo -e "  -a (Optional) Path to the architecture file. Required if -b is set. "
    echo -e "  -f (Optional) Path to the FPGA directory that contains the target FPGA bitstreams."
    echo -e "      This is a REQUIRED argument if at least one of the following conditions is met: "
    echo -e "      1. -u is set, i.e., update SD card with the bitstreams in this directory at the end."
    echo -e "         The directory must contain top.core.rbf top.periph.rbf;"
    echo -e "      2. -b is set, i.e., build bitstream and save to this location"
    echo -e "  -y (Optional) Path to a pre-built Yocto image directory.   If given, this script skips"
    echo -e "     building the Yocto image from scratch and use the .wic image and the poky SDK file from"
    echo -e "     this build directory instead"
    echo -e "  -m (Optional) FPGA Machine.  Options: arria10 (Arria 10 SoC), agilex7_dk_si_agi027fa (Agilex 7 SoC),"
    echo -e "     agilex5_modular (Agilex 5 SoC). Default: arria10"
    echo
}

function clean_runtime()
{
    echo "Cleaning runtime directory"

    echo "rm -rf ${RUNTIME_BUILD_DIR}"
    rm -rf ${RUNTIME_BUILD_DIR}

    echo "rm -rf ${RUNTIME_DIR}/embedded_arm_sdk"
    rm -rf ${RUNTIME_DIR}/embedded_arm_sdk

    echo "rm -rf ${HPSPACKAGES_BUILD_DIR}"
    rm -rf ${HPSPACKAGES_BUILD_DIR}

    # Search for presence of Poky SDK file
    ED4_POKY_SDK_NAME="poky*.sh"
    ED4_POKY_SDK_FILE_LOC="$(find ${RUNTIME_DIR} -maxdepth 1 -type f -name ${ED4_POKY_SDK_NAME})"

    # Confirm presence of Poky SDK file and remove it
    if [ -e "${ED4_POKY_SDK_FILE_LOC}" ]; then
        echo "rm -rf ${ED4_POKY_SDK_FILE_LOC}"
        rm -rf ${ED4_POKY_SDK_FILE_LOC}
    fi
}

while getopts "hcebuf:o:y:m:a:" optname; do
    case "$optname" in
        h)
            usage
            exit 0
            ;;
        c)
            clean_runtime
            exit 0
            ;;
        f)
            if [[ ${OPTARG} != /* ]]; then
                ED4_BITSTREAM_DIR="$(pwd)/${OPTARG}"
            else
                ED4_BITSTREAM_DIR=${OPTARG}
            fi
            ;;
        e)
            BUILD_EXPERIMENT_FEATURE=true
            ;;
        o)
            if [[ ${OPTARG} != /* ]]; then
                ED4_SDCARD_DIR="$(pwd)/${OPTARG}"
            else
                ED4_SDCARD_DIR=${OPTARG}
            fi
            ED4_ROOTFS_DIR="${ED4_SDCARD_DIR}/ed4_root"
            ED4_APP_DIR="${ED4_ROOTFS_DIR}/home/root/app"
            ;;
        u)
            UPDATE_SDCARD=true
            ;;
        b)
            BUILD_BITSTREAM=true
            ;;
        y)
            if [[ ${OPTARG} != /* ]]; then
                YOCTO_BUILD_DIR="$(pwd)/${OPTARG}"
            else
                YOCTO_BUILD_DIR=${OPTARG}
            fi
            USING_PREBUILT_YOCTO=true
            ;;
        m)
            MACHINE=${OPTARG}
            if ! [[ "${MACHINE}" == "agilex5_modular" || "${MACHINE}" == "agilex7_dk_si_agi027fa" || "${MACHINE}" == "stratix10" || "${MACHINE}" == "arria10" ]]; then
                usage
                exit 1
            fi
            ;;
        a)
            if [[ ${OPTARG} != /* ]]; then
                ARCH_FILE="$(pwd)/${OPTARG}"
            else
                ARCH_FILE=${OPTARG}
            fi
            ;;
    esac
done

if [[ -z ${ED4_SDCARD_DIR} ]]; then
    usage
    echo "Error: -o is required"
    exit 1;
fi

if [[ "${UPDATE_SDCARD}" == true && ! -d ${ED4_BITSTREAM_DIR} && "${BUILD_BITSTREAM}" == false ]]; then
    usage
    echo "Error: -f is required and must exist if -u is set. "\
         "Add -b if you want to build bitstreams"
    exit 1;
fi

if [[ "${BUILD_BITSTREAM}" == true ]]; then
    if [[ -z ${ED4_BITSTREAM_DIR} ]]; then
        usage
        echo "Error: -f must be specified if building a bitstream"
        exit 1;
    fi
    if [[ -z ${ARCH_FILE} ]]; then
        usage
        echo "Error: -a must be specified if building a bitstream"
        exit 1;
    fi
    if [[ -d ${ED4_BITSTREAM_DIR} && "$(ls -A ${ED4_BITSTREAM_DIR})" ]]; then
        echo "Error: ${ED4_BITSTREAM_DIR} is not empty. "
        exit 1;
    fi
fi

function build_s2m_bitstream()
{
    if [ ! -f "${BUILD_BITSTREAM_SCRIPT}" ]; then
        echo "Error: Cannot find ${BUILD_BITSTREAM_SCRIPT}."
        exit 1
    fi

    echo "Building bitstream for ${MACHINE}"

    ed="a10_soc_s2m"
    if [ "${MACHINE}" == "stratix10" ]; then
        ed="s10_soc_s2m"
    elif [ "${MACHINE}" == "agilex7_dk_si_agi027fa" ]; then
        ed="agx7_soc_s2m"
    elif [ "${MACHINE}" == "agilex5_modular" ]; then
        ed="agx5_soc_s2m"
    fi

    ${BUILD_BITSTREAM_SCRIPT} build -o ${ED4_BITSTREAM_DIR} -n 1 ${ed} ${ARCH_FILE}
    BUILD_BITSTREAM_RESULT=$?
    if [ $BUILD_BITSTREAM_RESULT -eq 1 ]; then
        echo "Bitstream failed to build. "
        exit 1
    fi

    if [ "${MACHINE}" == "arria10" ]; then
        if [[ $(find ${ED4_BITSTREAM_DIR} -maxdepth 1 -name '*.periph.rbf' | wc -l) -eq 1 &&
            $(find ${ED4_BITSTREAM_DIR} -maxdepth 1 -name '*.core.rbf' | wc -l) -eq 1 ]]; then
            periph=$(find ${ED4_BITSTREAM_DIR} -maxdepth 1 -name '*.periph.rbf')
            core=$(find ${ED4_BITSTREAM_DIR} -maxdepth 1 -name '*.core.rbf')
            mv $periph "${ED4_BITSTREAM_DIR}/top.periph.rbf"
            mv $core "${ED4_BITSTREAM_DIR}/top.core.rbf"
        else
            echo "Error: You should have exactly 1 periph.rbf and 1 core.rbf in ${ED4_BITSTREAM_DIR}"
            exit 1
        fi
    else
        if [[ $(find ${ED4_BITSTREAM_DIR} -maxdepth 1 -name '*.sof' | wc -l) -eq 1 ]]; then
            top=$(find ${ED4_BITSTREAM_DIR} -maxdepth 1 -name '*.sof')
            mv $top "${ED4_BITSTREAM_DIR}/top.sof"
        else
            echo "Error: You should have exactly 1 .sof file in ${ED4_BITSTREAM_DIR}"
            exit 1
        fi
    fi
}

function build_yocto()
{
    if [ ! -f "${YOCTO_BUILD_SCRIPT}" ]; then
        echo "Error: Cannot find run-build.sh at ${YOCTO_DIR}."
        exit 1
    fi
    echo "Building Yocto for ${MACHINE}"
    if [[ "${USING_PREBUILT_YOCTO}" == false ]]; then
        umask a+rx u+rwx
    fi
    pushd $YOCTO_DIR
        # -i: build_image; -s: build_sdk, -b <build_directory>: build location
        if [[ "${USING_PREBUILT_YOCTO}" == false ]]; then
            ${YOCTO_BUILD_SCRIPT} -is -b ${YOCTO_BUILD_DIR} ${MACHINE}
        else
            echo "Using prebuilt Yocto: ${YOCTO_BUILD_DIR}"
        fi
        YOCTO_BUILD_RESULT=$?
        if [ $YOCTO_BUILD_RESULT -eq 0 ]; then
            echo "Yocto built successfully. "
            YOCTO_SDK_DIR="${YOCTO_BUILD_DIR}/build/tmp/deploy/sdk"
            export SEARCH_NAME="poky*${MACHINE}*.sh"
            export ED4_POKY_SDK_LOC="$(find ${YOCTO_SDK_DIR} -name ${SEARCH_NAME})"
            if [ ! -f "${ED4_POKY_SDK_LOC}" ]; then
                echo "Error: Cannot find the POKY SDK in ${YOCTO_SDK_DIR}. "
                exit 1
            else
                echo "The POKY SDK can be found at ${ED4_POKY_SDK_LOC}"
            fi

            # Newer versions of Yocto generate a symlink to the .wic file with an extension of .rootfs.wic
            # instead of just .wic. To keep everything working, make a copy of the file without the .rootfs extension.
            YOCTO_WIC_DIR="${YOCTO_BUILD_DIR}/build/tmp/deploy/images/${MACHINE}"
            YOCTO_WIC_FILE="coredla-image-${MACHINE}.wic"
            YOCTO_ROOTFS_WIC_FILE="coredla-image-${MACHINE}.rootfs.wic"

            # Check if the .wic file is missing
            if [ ! -f "${YOCTO_WIC_DIR}/${YOCTO_WIC_FILE}" ]; then

                # Check if the .rootfs.wic file is present
                if [ -f "${YOCTO_WIC_DIR}/${YOCTO_ROOTFS_WIC_FILE}" ]; then
                    cp -a "${YOCTO_WIC_DIR}/${YOCTO_ROOTFS_WIC_FILE}" "${YOCTO_WIC_DIR}/${YOCTO_WIC_FILE}"
                else
                    echo "Error: ${YOCTO_WIC_DIR}/${YOCTO_ROOTFS_WIC_FILE} missing"
                    exit 1
                fi
            fi

            WIC_COPY_RESULT=$?
            if [ $WIC_COPY_RESULT -eq 0 ]; then
                echo "The .wic file can be found at ${YOCTO_WIC_DIR}/${YOCTO_WIC_FILE}"
            else
                echo "Error: ${YOCTO_WIC_DIR}/${YOCTO_WIC_FILE} missing"
                exit 1
            fi

            # The same issue as above applies to the .cpio file
            YOCTO_CPIO_FILE="coredla-image-${MACHINE}.cpio"
            YOCTO_ROOTFS_CPIO_FILE="coredla-image-${MACHINE}.rootfs.cpio"

            # Check if the .cpio file is missing
            if [ ! -f "${YOCTO_WIC_DIR}/${YOCTO_CPIO_FILE}" ]; then

                # Check if the .rootfs.cpio file is present
                if [ -f "${YOCTO_WIC_DIR}/${YOCTO_ROOTFS_CPIO_FILE}" ]; then
                    cp -a "${YOCTO_WIC_DIR}/${YOCTO_ROOTFS_CPIO_FILE}" "${YOCTO_WIC_DIR}/${YOCTO_CPIO_FILE}"
                else
                    echo "Error: ${YOCTO_WIC_DIR}/${YOCTO_ROOTFS_CPIO_FILE} missing"
                    exit 1
                fi
            fi

            CPIO_COPY_RESULT=$?
            if [ $CPIO_COPY_RESULT -eq 0 ]; then
                echo "The .cpio file can be found at ${YOCTO_WIC_DIR}/${YOCTO_CPIO_FILE}"
            else
                echo "Error: ${YOCTO_WIC_DIR}/${YOCTO_CPIO_FILE} missing"
                exit 1
            fi

        else
            echo "Yocto failed to build"
            exit 1
        fi
    popd
}

function build_hpspackages()
{
    if [ ! -f "${BUILD_HPSPACKAGES_SCRIPT}" ]; then
        echo "Error: Cannot find build_hpspackages.sh at ${RUNTIME_DIR}."
        exit 1
    fi

    echo "Building hps packages..."
    pushd $RUNTIME_DIR
        if [[ $RUNTIME_BUILD_TYPE == "Release" ]]; then
            ${BUILD_HPSPACKAGES_SCRIPT} -bs  # -b: build; -s: get sources
        elif [[ $RUNTIME_BUILD_TYPE == "Debug" ]]; then
            ${BUILD_HPSPACKAGES_SCRIPT} -bds  # -b: build; -s: get sources -d: build openvino with cmake debug
        fi
        HPSPACKAGES_BUILD_RESULT=$?
        if [ $HPSPACKAGES_BUILD_RESULT -eq 0 ]; then
            echo "HPS packages built successfully. "
        else
            echo "HPS packages failed to build. "
            exit 1
        fi
    popd

}

function build_experimental() {
    # Check if the build script exists
    if [ ! -f "${BUILD_EXPERIMENTAL_SCRIPT}" ]; then
        echo "Error: Cannot find ${BUILD_EXPERIMENTAL_SCRIPT}. Build experimental features is internal only."
        exit 1
    fi

    echo "Building experiment target..."

    pushd $RUNTIME_DIR

        # Execute the build script
        ${BUILD_EXPERIMENTAL_SCRIPT}  # add -d for developer option
        BUILD_EXPERIMENTAL_RESULT=$?

        # Check the result
        if [ $BUILD_EXPERIMENTAL_RESULT -eq 0 ]; then
            echo "Experiment target built successfully."
        else
            echo "Experiment target failed to build."
            exit 1
        fi

    popd
}

function build_hps_dla_runtime()
{
    if [ ! -f "${BUILD_RUNTIME_SCRIPT}" ]; then
        echo "Error: Cannot find build_runtime.sh at ${RUNTIME_DIR}."
        exit 1
    fi

    # arm32 or arm64 platform depends on arria10 or stratix10/agilex7/agilex5
    ARM_ARCH="armv7l"
    if [[ "${MACHINE}" == "agilex5_modular" || "${MACHINE}" == "agilex7_dk_si_agi027fa" || "${MACHINE}" == "stratix10" ]]; then
        ARM_ARCH="aarch64"
    fi

    echo "Building runtime with the HPS platform..."
    pushd $RUNTIME_DIR
        if [[ $RUNTIME_BUILD_TYPE == "Release" ]]; then
            ${BUILD_RUNTIME_SCRIPT} --hps_platform --hps_machine=${MACHINE} --build_dir=${RUNTIME_BUILD_DIR}
        elif [[ $RUNTIME_BUILD_TYPE == "Debug" ]]; then
            ${BUILD_RUNTIME_SCRIPT} --hps_platform --hps_machine=${MACHINE} --build_dir=${RUNTIME_BUILD_DIR} -cmake_debug
        else
            echo "Unrecognized build type ${RUNTIME_BUILD_TYPE}. "
            exit 1
        fi

        RUNTIME_BUILD_RESULT=$?
        if [ $RUNTIME_BUILD_RESULT -eq 0 ]; then
            echo "DLA runtime built successfully. "
        else
            echo "DLA runtime failed to build. "
            exit 1
        fi
    popd

    # extract executable and libraries for ED4
    mkdir -p "${ED4_APP_DIR}"

    if [[ "${BUILD_EXPERIMENT_FEATURE}" == true ]]; then
        mkdir -p "${ED4_APP_DIR}/.experimental"
    fi

    pushd $ED4_APP_DIR
        # rsync is used for convenience to select/exclude files destined for the SD card
        rsync -avzP  ${RUNTIME_BUILD_DIR}/dla_benchmark/dla_benchmark .

        rsync -avzP  ${HPSPACKAGES_BUILD_DIR}/openvino/bin/${ARM_ARCH}/${RUNTIME_BUILD_TYPE}/*.so* .
        rsync -avzP  ${HPSPACKAGES_BUILD_DIR}/openvino/bin/${ARM_ARCH}/Release/libopenvino_arm_cpu_plugin.so .

        rsync -avzP  ${RUNTIME_BUILD_DIR}/common/format_reader/libformat_reader.so .
        rsync -avzP  ${RUNTIME_BUILD_DIR}/coredla_device/mmd/hps_platform/libhps_platform_mmd.so .
        rsync -avzP  ${RUNTIME_BUILD_DIR}/hetero_plugin/libcoreDLAHeteroPlugin.so .
        rsync -avzP  ${RUNTIME_BUILD_DIR}/hetero_plugin/libcoreDLAHeteroPlugin.so ./hetero_plugin/
        rsync -avzP  ${RUNTIME_BUILD_DIR}/libcoreDlaRuntimePlugin.so .
        rsync -avzP  ${RUNTIME_BUILD_DIR}/plugins.xml .

        rsync -avzP  --exclude libopencv_highgui.so \
                    --exclude libopencv_core.so \
                    --exclude libopencv_imgcodecs.so \
                    --exclude libopencv_imgproc.so \
                    --exclude libopencv_videoio.so \
                    ${HPSPACKAGES_BUILD_DIR}/armcpu_package/opencv/lib/*.so* .

        rsync -avzP ${RUNTIME_BUILD_DIR}/streaming/image_streaming_app/image_streaming_app .
        rsync -avzP ${RUNTIME_BUILD_DIR}/streaming/streaming_inference_app/streaming_inference_app .
        rsync -avzP ${RUNTIME_DIR}/streaming/runtime_scripts/run_image_stream.sh .
        rsync -avzP ${RUNTIME_DIR}/streaming/runtime_scripts/run_inference_stream.sh .
        rsync -avzP ${RUNTIME_DIR}/streaming/streaming_inference_app/categories.txt .

        if [[ "${BUILD_EXPERIMENT_FEATURE}" == true ]]; then
            rsync -avzP ${RUNTIME_DIR}/arm_build/coredla/dla/bin/dlac ./.experimental
            rsync -avzP ${RUNTIME_DIR}/arm_build/coredla/dla/lib/plugins.xml ./.experimental/plugins_aot_arm.xml
            rsync -avzP ${RUNTIME_DIR}/arm_build/coredla/dla/lib/libcoreDLAAotPlugin.so ./.experimental/
            rsync -avzP ${RUNTIME_DIR}/arm_build/coredla/dla/lib/libarchparam.so ./.experimental/
            rsync -avzP ${RUNTIME_DIR}/arm_build/coredla/dla/lib/liblpsolve*.so ./.experimental/
            rsync -avzP ${RUNTIME_DIR}/arm_build/coredla/dla/lib/libdla_compiler_core.so ./.experimental/
            # change the CPU plugin in plugins.xml to armPlugin
            sed -i "s/libopenvino_intel_cpu_plugin.so/libopenvino_arm_cpu_plugin.so/" ./.experimental/plugins_aot_arm.xml
        fi
        rsync -avzP ${RUNTIME_DIR}/streaming/streaming_inference_app/categories.txt .

        rsync -avzP ${COREDLA_ROOT}/build_version.txt .
        rsync -avzP ${COREDLA_ROOT}/build_os.txt .
    popd

    # Derive the .arch file from the selected MACHINE
    # Note: If the bitstream was built with an .arch file other than Performance then
    # run_inference_stream.sh will have to be updated manually
    ARCH_FILE_NAME="A10_Performance.arch"
    if [ "${MACHINE}" == "agilex7_dk_si_agi027fa" ]; then
        ARCH_FILE_NAME="AGX7_Performance_LayoutTransform.arch"
    elif [ "${MACHINE}" == "stratix10" ]; then
        ARCH_FILE_NAME="S10_Performance.arch"
    elif [ "${MACHINE}" == "agilex5_modular" ]; then
        ARCH_FILE_NAME="AGX5_Performance.arch"
    fi

    # update the -arch flag in run_inference_stream.sh to match the selected MACHINE
    sed -i "s/A10_Performance.arch/$ARCH_FILE_NAME/" $ED4_APP_DIR/run_inference_stream.sh

    if [ "${MACHINE}" == "agilex7_dk_si_agi027fa" ]; then
        # Append flag to image_streaming_app call to skip external layout transform on AGX7.
        sed -i 's|-rate=50|& -skip_external_transform|' $ED4_APP_DIR/run_image_stream.sh

        # Change run_inference_stream to use RN50_Performance_b1.bin instead of RN50_Performance_no_folding.bin
        # AGX7 is the only example design that supports folding
        sed -i "s/RN50_Performance_no_folding.bin/RN50_Performance_b1.bin/" $ED4_APP_DIR/run_inference_stream.sh

        # Delete the comment about no folding
        sed -i "/# The model must be compiled with no folding/d" $ED4_APP_DIR/run_inference_stream.sh
    fi

    # change the CPU plugin in plugins.xml to armPlugin
    sed -i "s/libopenvino_intel_cpu_plugin.so/libopenvino_arm_cpu_plugin.so/" $ED4_APP_DIR/plugins.xml
}

function update_sd_card_image()
{
    if [ ! -f "${UPDATE_SDCARD_SCRIPT}" ]; then
        echo "Error: Cannot find update_sd_card.sh at ${ED4_SCRIPT_DIR}."
        exit 1
    fi
    export SDCARD_IMAGE_LOC="$(find ${YOCTO_BUILD_DIR}/build/tmp/deploy/images -name *coredla-image-${MACHINE}.wic)"
    if [ ! -f "${SDCARD_IMAGE_LOC}" ]; then
        echo "Error: Cannot find SD Card image (.wic) at ${YOCTO_BUILD_DIR}/build/tmp/deploy/images. Have you built Yocto?."
        exit 1
    fi

    echo "Updating SD Card image..."
    pushd $ED4_SCRIPT_DIR
        ${UPDATE_SDCARD_SCRIPT} -w ${SDCARD_IMAGE_LOC} -o ${ED4_SDCARD_DIR} -f ${ED4_BITSTREAM_DIR} -r ${ED4_ROOTFS_DIR}
        UPDATE_SDCARD_RESULT=$?
        if [ $UPDATE_SDCARD_RESULT -eq 0 ]; then
            echo "Updated SD card image successfully. "
        else
            echo "Failed to update SD card image. "
            exit 1
        fi
    popd

    if [[ $(find ${ED4_SDCARD_DIR} -name '*.wic' | wc -l) ]]; then
        echo "The .wic image can be found here:"
        echo $(find ${ED4_SDCARD_DIR} -name '*.wic')
    else
        echo "Cannot find updated .wic image in ${ED4_SDCARD_DIR}"
        exit 1
    fi
}
#################################################################


# step 0: build bitstream
if [[ "${BUILD_BITSTREAM}" == true ]]; then
    build_s2m_bitstream
fi
# step 1: build Yocto
build_yocto
# step 2: build hps packages
build_hpspackages
# step 3: build experimental features
if [[ "${BUILD_EXPERIMENT_FEATURE}" == true ]]; then
    build_experimental
fi
# step 3: build coredla runtime on the hps platform
build_hps_dla_runtime
# step 4: update the .wic image from step 1 with hps runtime and fpga bitstreams
if [[ "${UPDATE_SDCARD}" == true ]]; then
    update_sd_card_image
fi

echo "All steps succeeded"

# **How to use PCIe-Based Design Example** 

A PCIe-based example design for Agilex 7 devices is a reference hardware and software project provided by Intel that demonstrates how to implement PCI Express communication between an Intel Agilex 7 FPGA and a host system (usually an x86 server or workstation) over the PCIe interface.

# first download the FPGA AI SUITE from this link 
https://www.intel.com/content/www/us/en/software-kit/current/775147/

# Installing the FPGA AI SUITE commands 
sudo apt install --reinstall libappstream4
sudo apt update
sudo apt install <full path to fpga-ai-suite.deb>

# creating a virtual environment 
python3.12 -m venv openvino_env
source openvino_env/bin/activate

# Installing OpenVino toolkit 
download the tar file from the link : 
https://storage.openvinotoolkit.org/repositories/openvino/packages/2023.3/linux/

move to the downloaded file location and run this command 
curl -L https://storage.openvinotoolkit.org/\
repositories/openvino/packages/2024.6/linux/\
l_openvino_toolkit_ubuntu22_2024.6.0.17404.4c0f47d2335_x86_64.tgz \
--output openvino_2024.6.0.tgz
tar -xf openvino_2024.6.0.tgz
sudo mv \
l_openvino_toolkit_ubuntu22_2024.6.0.17404.4c0f47d2335_x86_64 \
/opt/intel/openvino_2024.6.0

# Installing the system dependencies 
cd /opt/intel/openvino_2024.6.0/install_dependencies/
sudo -E ./install_openvino_dependencies.sh

# Create a symbolic link 
## what is a symbolic link?

Think of a symlink as a shortcut or alias to another directory or file.

    Actual directory: /opt/intel/openvino_2024.6.0

    Symlink created: /opt/intel/openvino_2024 → points to openvino_2024.6.0

cd /opt/intel
sudo ln -s openvino_2024.6.0 openvino_2024

# Installing pytorch and tensorflow 
pip install "openvino-dev[caffe, pytorch, tensorflow]==2024.6.0"

# PyTorch 2.6.0 is not supported by the OpenVINO 2024.6.0 Open
# Model Zoo tools. Change your installed PyTorch version to 2.5.1
# with the following commands:
pip freeze | grep torch
pip uninstall torch==2.6.0 torchvision==0.21.0
pip install torch==2.5.1 torchvision==0.20.1 \
--index-url https://download.pytorch.org/whl/cu118

# Setting up environment variables 
source /opt/intel/openvino_2024/setupvars.sh
source /opt/altera/fpga_ai_suite_2025.1/dla/setupvars.sh

# Verfying the installation by running the following command:
dla_compiler --fanalyze-area --march $COREDLA_ROOT/*/AGX7_Generic.arch

# Setting up environment variables 
source /opt/intel/openvino_2024/setupvars.sh
source /opt/altera/fpga_ai_suite_2025.1/dla/setupvars.sh

# Creating and setting up working directory
echo "[INFO] Creating working directory..."
mkdir -p /root/DE10-Agilex/coredla_work3
cd /root/DE10-Agilex/coredla_work3


echo "[INFO] Initializing DLA local directory..."
source dla_init_local_directory.sh

# Downloading OpenVINO model zoo
echo "[INFO] Cloning Open Model Zoo..."
cd "$COREDLA_WORK/demo"
git clone https://github.com/openvinotoolkit/open_model_zoo.git
cd open_model_zoo


# Activating OpenVINO Python environment
echo "[INFO] Activating OpenVINO environment..."
source /root/DE10-Agilex/openvino_env/bin/activate

# Download and convert model
echo "[INFO] Downloading and converting resnet-50-tf..."
omz_downloader --name resnet-50-tf --output_dir "$COREDLA_WORK/demo/models/"
omz_converter --name resnet-50-tf --download_dir "$COREDLA_WORK/demo/models/" --output_dir "$COREDLA_WORK/demo/models/"

# Running DLA compiler
echo "[INFO] Compiling model with dla_compiler..."
cd "$COREDLA_WORK/demo/models/public/resnet-50-tf/FP32"
dla_compiler \
  --march "$COREDLA_ROOT/example_architectures/AGX7_Performance.arch" \
  --network-file ./resnet-50-tf.xml \
  --foutput-format=open_vino_hetero \
  --o "$COREDLA_WORK/demo/RN50_Performance_b1.bin" \
  --batch-size=1 \
  --fanalyze-performance

# Building runtime
echo "[INFO] Building runtime..."
cd "$COREDLA_WORK/runtime"
rm -rf build_Release
./build_runtime.sh -target_de10_agilex

# Programming Agilex 7 FPGA
echo "[INFO] Programming FPGA with bitstream..."
jtagdir="$COREDLA_WORK/runtime/build_Release/fpga_jtag_reprogram"
bitsdir="$COREDLA_WORK/demo/bitstreams"
"$jtagdir/fpga_jtag_reprogram" "$bitsdir/AGX7_Performance_DE10_Agilex.sof"

# Running the model
echo "[INFO] Running the model on FPGA+CPU (HETERO)..."
curarch="$COREDLA_ROOT/example_architectures/AGX7_Performance.arch"
imagedir="$COREDLA_WORK/demo/sample_images"
xmldir="$COREDLA_WORK/demo/models/public"

"$COREDLA_WORK/runtime/build_Release/dla_benchmark/dla_benchmark" \
  -b=1 \
  -m "$xmldir/resnet-50-tf/FP32/resnet-50-tf.xml" \
  -d=HETERO:FPGA,CPU \
  -niter=8 \
  -plugins "/root/DE10-Agilex/plugins.xml" \
  -arch_file "$curarch" \
  -api=async \
  -perf_est \
  -nireq=4 \
  -bgr \
  -i "$imagedir" \
  -groundtruth_loc "$imagedir/TF_ground_truth.txt"

# important paths 
# bashrc
export QSYS_ROOTDIR="/opt/intel/oneapi/intelfpgadpcpp/2021.4.0/QuartusPrimePro/21.2/qsys/bin"
export AOCL_BOARD_PACKAGE_ROOT="/opt/intel/oneapi/intelfpgadpcpp/2021.4.0/board/de10_agilex"
export QUARTUS_ROOTDIR="/opt/intel/oneapi/intelfpgadpcpp/2021.4.0/QuartusPrimePro/21.2/quartus/bin"

export PATH="$PATH:"/opt/intel/oneapi/intelfpgadpcpp/2021.4.0/QuartusPrimePro/21.2/qsys/bin/
export PATH="$PATH:"/opt/intel/oneapi/intelfpgadpcpp/2021.4.0/QuartusPrimePro/21.2/quartus/bin/

/root/.bashrc

# plugins.xml
<ie>
    <plugins>
        <plugin name="AUTO" location="${CMAKE_SHARED_LIBRARY_PREFIX}openvino_auto_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}"></plugin>
        <plugin name="BATCH" location="${CMAKE_SHARED_LIBRARY_PREFIX}openvino_auto_batch_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}"></plugin>
        <plugin name="CPU" location="/opt/intel/openvino_2024.6.0/runtime/lib/intel64/libopenvino_intel_cpu_plugin.so"></plugin>
        <plugin name="GNA" location="${CMAKE_SHARED_LIBRARY_PREFIX}openvino_intel_gna_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}"></plugin>
        <plugin name="GPU" location="${CMAKE_SHARED_LIBRARY_PREFIX}openvino_intel_gpu_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}"></plugin>
        <plugin name="MULTI" location="${CMAKE_SHARED_LIBRARY_PREFIX}openvino_auto_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}"></plugin>
        <plugin name="MYRIAD" location="${CMAKE_SHARED_LIBRARY_PREFIX}openvino_intel_myriad_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}"></plugin>
        <!-- plugins location must point to absolute or relative path for CoreDLA plugins -->
        <plugin name="HETERO" location="/opt/intel/openvino_2024.6.0/runtime/lib/intel64/libopenvino_hetero_plugin.so"></plugin>
        <plugin name="FPGA" location="/root/DE10-Agilex/coredla_work3/runtime/build_Release/libcoreDlaRuntimePlugin.so"></plugin>
    </plugins>
</ie>





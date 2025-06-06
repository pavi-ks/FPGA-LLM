#!/bin/bash

set -e  # Exit immediately if any command fails
set -o pipefail


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


echo "[ DONE] Complete DLA pipeline executed successfully."

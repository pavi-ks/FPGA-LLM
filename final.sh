#! /bin/bash

#creating a working directory 

mkdir ~/coredla_work
cd ~/coredla_work
source dla_init_local_directory.sh

# setting environment  variables

mkdir ~/coredla_work
cd ~/coredla_work
source dla_init_local_directory.sh


# Downloading  model zoo
cd $COREDLA_WORK/demo
git clone https://github.com/openvinotoolkit/open_model_zoo.git
cd open_model_zoo
git checkout 2024.6.0

#preparing the model 
source /root/DE10-agilex/openvino_env/bin/activate
omz_downloader --name resnet-50-tf \
--output_dir $COREDLA_WORK/demo/models/
omz_converter --name resnet-50-tf \
--download_dir $COREDLA_WORK/demo/models/ \
--output_dir $COREDLA_WORK/demo/models/

#running the graph compiler
cd $COREDLA_WORK/demo/models/public/resnet-50-tf/FP32
dla_compiler \
--march $COREDLA_ROOT/example_architectures/AGX7_Performance.arch \
--network-file ./resnet-50-tf.xml \
--foutput-format=open_vino_hetero \
--o $COREDLA_WORK/demo/RN50_Performance_b1.bin \
--batch-size=1 \
--fanalyze-performance

#programming the agilex 7  
cd $COREDLA_WORK/runtime
rm -rf build_Release
./build_runtime.sh -target_de10_agilex

#programming into bitstream 
jtagdir=$COREDLA_WORK/runtime/build_Release/fpga_jtag_reprogram
bitsdir=$COREDLA_WORK/demo/bitstreams
$jtagdir/fpga_jtag_reprogram $bitsdir/AGX7_Performance.sof
curarch=$COREDLA_ROOT/example_architectures/AGX7_Performance.arch


# Running the model 
imagedir=$COREDLA_WORK/demo/sample_images
xmldir=$COREDLA_WORK/demo/models/public/
$COREDLA_WORK/runtime/build_Release/dla_benchmark/dla_benchmark \
-b=1 \
-m $xmldir/resnet-50-tf/FP32/resnet-50-tf.xml \
-d=HETERO:FPGA,CPU \
-niter=8 \
-plugins $COREDLA_WORK/runtime/build_Release/plugins.xml \
-arch_file $curarch \
-api=async \
-perf_est \
-nireq=4 \
-bgr \
-i $imagedir \
-groundtruth_loc $imagedir/TF_ground_truth.txt
jtagdir=$COREDLA_WORK/runtime/build_Release/fpga_jtag_reprogram
bitsdir=$COREDLA_WORK/demo/bitstreams
$jtagdir/fpga_jtag_reprogram $bitsdir/AGX7_Performance.sof
curarch=$COREDLA_ROOT/example_architectures/AGX7_Performance.arch

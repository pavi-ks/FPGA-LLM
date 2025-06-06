#!/bin/bash
curarch=$COREDLA_ROOT/example_architectures/AGX7_Performance.arch
imagedir=$COREDLA_WORK/demo/sample_images
xmldir=$COREDLA_WORK/demo/models/public
$COREDLA_WORK/runtime/build_Release/dla_benchmark/dla_benchmark \
-b=1 \
-m $xmldir/resnet-50-tf/FP32/resnet-50-tf.xml \
-d=HETERO:FPGA,CPU \
-niter=8 \
-plugins plugins.xml \
-arch_file $curarch \
-api=async \
-perf_est \
-nireq=4 \
-bgr \
-i $imagedir \
-groundtruth_loc $imagedir/TF_ground_truth.txt

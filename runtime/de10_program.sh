#!/bin/bash

jtagdir=$COREDLA_WORK/runtime/build_Release/fpga_jtag_reprogram
bitsdir=$COREDLA_WORK/demo/bitstreams
$jtagdir/fpga_jtag_reprogram $bitsdir/AGX7_Performance_DE10_Agilex.sof
curarch=$COREDLA_ROOT/example_architectures/AGX7_Performance.arch

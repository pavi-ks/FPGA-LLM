// Copyright (C) 2018-2023 Altera Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <gflags/gflags.h>
#include <iostream>
#include <string>
#include <vector>

/// @brief message for help argument
static const char help_message[] = "Print a usage message";

/// @brief message for images argument
static const char input_message[] =
    "Optional. Path to a folder with images and/or binaries or to specific image or binary file.";

/// @brief message for model argument
static const char model_message[] =
    "Required unless running the ahead-of-time flow using -cm. Path to an .xml file with a trained model";

static const char network_file_alias_message[] = "Required unless -m or -cm is present. Alias for -m";

/// @brief message for compiled model argument
static const char compiled_model_message[] = "Optional. Path to a .bin file with a trained compiled model";

/// @brief message for execution mode
static const char api_message[] = "Optional. Enable Sync/Async API. Default value is \"async\".";

/// @brief message for compile/inference device type.
static const char target_device_message[] =
    "Optional. Specify a target device to infer on Use \"-d HETERO:<comma-separated_devices_list>\" format to specify HETERO plugin. ";

/// @brief message for iterations count
/** static const char iterations_count_message[] = "Optional. Number of iterations. " \
"If not specified, the number of iterations is calculated depending on a device."; **/
static const char iterations_count_message[] = "Required. Number of iterations.";

/// @brief message for requests count
static const char infer_requests_count_message[] =
    "Optional. Number of infer requests. Default value is determined automatically for device.";

/// @brief message for #threads for CPU inference
static const char infer_num_threads_message[] =
    "Optional. Number of threads to use for inference on the CPU "
    "(including HETERO).";

/// @brief message for #streams for CPU inference
static const char infer_num_streams_message[] =
    "Optional. Number of streams to use for inference on the CPU in throughput mode "
    "(for HETERO device cases use format <dev1>:<nstreams1>,<dev2>:<nstreams2> or just <nstreams>). "
    "Default value is determined automatically for a device. Please note that although the automatic selection "
    "usually provides a reasonable performance, it still may be non - optimal for some cases, especially for "
    "very small networks. See sample's README for more details.";

/// @brief message for user library argument
static const char custom_cpu_library_message[] =
    "Required for CPU custom layers. Absolute path to a shared library with the kernels implementations.";

static const char batch_size_message[] =
    "Optional. Batch size value. If not specified, the batch size value is determined from Intermediate "
    "Representation.";

static const char batch_size_alias_message[] = "Optional. Alias for -b.";

static const char min_subgraph_layers_message[] =
    "Optional. Minimum number of layers allowed in a subgraph that runs on FPGA. Subgraph with fewer"
    " layers than this value will run on CPU in Hetero plugin. Must be >= 1";

/// @brief message for CPU threads pinning option
static const char infer_threads_pinning_message[] =
    "Optional. Enable threads->cores (\"YES\", default), threads->(NUMA)nodes (\"NUMA\") "
    "or completely disable (\"NO\") "
    "CPU threads pinning for CPU-involved inference.";

/// @brief message for stream_output option
static const char stream_output_message[] =
    "Optional. Print progress as a plain text. When specified, an interactive progress bar is replaced with a "
    "multiline output.";

/// @brief message for the save_run_summary option
static const char save_run_summary_message[] =
    "Optional. Enable saving a summary of the run containing the "
    "specified command line parameters and a copy of the performance report "
    "printed to stdout.";

/// @brief message for report_folder option
static const char report_folder_message[] = "Optional. Path to a folder where statistics report is stored.";

// @brief message for progress bar option
static const char progress_message[] =
    "Optional. Show progress bar (can affect performance measurement). Default values is \"false\".";

/// @brief message for the custom plugins.xml file option
static const char plugins_message[] = "Optional. Select a custom plugins_xml file to use. "
    "-plugins=emulation to use xml file for software emulation";

/// @brief message for the custom plugins_xml_file.xml file option
static const char old_plugins_message[] =
    "***DEPRECATED OPTION*** Please use NEW -plugins option to specify which custom plugins xml file to use";

/// @brief message for ground truth file
static const char groundtruth_loc_message[] =
    "Optional. Select a ground truth file to use for calculating top 1 top 5 results.";

/// @brief message for architecture .arch file
static const char arch_file_message[] = "Optional. Provide a path for the architecture .arch file.";

/// @brief message for --arch flag.
static const char arch_alias_message[] = "Optional. Alias for -arch_file.";

/// @brief message performance estimation
static const char perf_est_message[] = "Optional. Perform performance estimation.";

/// @brief message folding_option flag
static const char folding_option_message[] = "Optional. Set the folding options for dla compiler: options 0-3.";

/// @brief message fold_preprocessing flag
static const char fold_preprocessing_message[] = "Optional. Enable fold preprocessing option for dla compiler.";

/// @brief message bgr flag
static const char bgr_message[] = "Optional. Indicate images are in bgr format.";

/// @brief message dump_output flag
static const char dump_output_message[] = "Optional. Dumps output of graph to result.txt and result.bin file(s).";

/// @brief message for output_dir option
static const char output_dir_message[] = "Optional. Path to a folder where result files are dumped to.";

/// @brief message for output_dir option
static const char dump_csr_message[] =
    "Optional. If set to true, then dumps FPGA AI Suite IP's CSR accesses in <current_working_directory>/csr_log.txt. "
    "Only available for AGX5 hostless designs. Default: true";

/// @brief message encryption_key flag
static const char encryption_key_message[] =
    "Optional. Encryption key (using hexidecimal characters, 16 bytes- 32 hexidecimal char).";

/// @brief message encryption_iv flag
static const char encryption_iv_message[] =
    "Optional. Initialization vector for encryption. (8 bytes - 16 hexidecimal char)";

/// @brief message debug network flag
static const char debug_network_message[] = "Optional. Dump the contents from the debug network.";

/// @brief message emulator_decryption flag
static const char emulator_decryption_message[] =
    "Optional. Set to true to enable decryption using emulator. Disable encryption in the import.";

/// @brief message hidden_help flag
static const char hidden_help_message[] = "Print help options that are experimental or for internal use.";

/// @brief message estimate_per_layer_latencies flag
static const char estimate_per_layer_latencies_message[] =
    "Optional. Estimates the number of cycles each layer will consume during execution based on the internal model "
    "Performance Estimator uses to estimate throughput. For internal use only.";

/// @brief message average_precision flag
static const char enable_object_detection_ap_message[] =
    "Optional. Set to true to show average precision and COCO average precision for YOLO graphs in the report.";

/// @brief message yolo_version flag
static const char yolo_version_message[] = "Optional. The version of the YOLO graph. Required for average precision report.";

/// @brief message binary flag
static const char bin_data_message[] =
    "Optional. Specify that the input should be read as binary data (otherwise, if input tensor has depth 1, or 3 it "
    "will default to U8 image processing).";

/// @brief message pc flag
static const char pc_message[] = "Optional. Report performance counters for the CPU subgraphs, if there is any.";

/// @brief message pcsort flag
static const char pcsort_message[] =
    "Optional. Report performance counters for the CPU subgraphs and analysis sort hotpoint opts. "
    "sort: Analysis opts time cost, print by hotpoint order; "
    "no_sort: Analysis opts time cost, print by normal order; "
    "simple_sort: Analysis opts time cost, only print EXECUTED opts by normal order.";

/// @brief message scale flag
static constexpr char input_image_scale_message[] =
    "Optional. Scale factors for each channel in [R, G, B] format. "
    "Applies normalization as (x - mean) / scale. "
    "Example: -scale_values input[1, 1, 1]. Not performed on FPGA.";

/// @brief message mean flag
static constexpr char input_image_mean_message[] =
    "Optional. Per-channel mean subtraction values in [R, G, B] format. "
    "Used for model input normalization as (x - mean) / scale. "
    "Example: -mean_values input[255,255,255]. Not performed on FPGA.";

/// @brief message resize flag
static const char input_image_resize_message[] =
    "Optional. Image resizing when the input image dimensions do not match the model."
    "'resize': Resizing the image to the model input size."
    "'pad_resize': Pad the image with zeros and resize to model input size.";

/// @brief message enable early-access features flag
static const char enable_early_access_message[] =
    "Optional. Enables early access (EA) features of FPGA AI Suite. These are features that are actively being "
    "developed and have not yet met production quality standards. These features may have flaws. "
    "Consult the FPGA AI Suite documentation for details.";

/// @brief message report LSU memory access count
static const char report_lsu_counters_message[] =
    "Optional. Report the number of memory accesses made by the "
    "input feature reader, output feature writer, and filter reader "
    "of each CoreDLA instance since device initialization. No report from the counters by default.";

/// @brief message for verbose flag
static const char verbose_message[] = "Optional. If true DLA Benchmark outputs detailed logs.";

/// @brief mesage for maximum file size flag
static const char output_output_file_size_message[] =
    "Optional. Maximum file size in MB that can be dumped to a txt. Used to avoid creating files that cannot be opened.";

/// @brief message for the DDR bandwidth
static const char ddr_bandwidth_message[] =
    "Optional. Specify the amount of available DDR bandwidth in MB/s for each instance of the FPGA AI Suite IP. "
    "Default value is device family-dependent. Only used for performance estimation.";

/// @brief Define flag for showing help message <br>
DEFINE_bool(h, false, help_message);

/// @brief Declare flag for showing help message <br>
DECLARE_bool(help);

/// @brief Define parameter for set image file <br>
/// i or mif is a required parameter
DEFINE_string(i, "", input_message);

/// @brief Define parameter for set model file <br>
/// It is a required parameter
DEFINE_string(m, "", model_message);

/// @brief Alias for -m
DEFINE_string(network_file, "", network_file_alias_message);

/// @brief Define parameter for compiled model file <br>
/// It is not a required parameter
DEFINE_string(cm, "", compiled_model_message);

/// @brief Define execution mode
DEFINE_string(api, "async", api_message);

/// @brief device the target device to infer on <br>
DEFINE_string(d, "", target_device_message);

/// @brief Absolute path to CPU library with user layers <br>
/// It is a required parameter
DEFINE_string(l, "", custom_cpu_library_message);

/// @brief Iterations count (default 0)
/// Sync mode: iterations count
/// Async mode: StartAsync counts
DEFINE_int32(niter, 0, iterations_count_message);

/// @brief Number of infer requests in parallel
DEFINE_int32(nireq, 0, infer_requests_count_message);

/// @brief Number of threads to use for inference on the CPU in throughput mode (also affects Hetero cases)
DEFINE_int32(nthreads, 0, infer_num_threads_message);

/// @brief Number of streams to use for inference on the CPU (also affects Hetero cases)
DEFINE_string(nstreams, "", infer_num_streams_message);

/// @brief Define parameter for batch size <br>
/// Default is 1
DEFINE_int32(b, 1, batch_size_message);

/// @brief alias for -b
DEFINE_int32(batch_size, 1, batch_size_alias_message);

/// @brief Minimum number of layers allowed in a subgraph that runs on FPGA
DEFINE_int32(min_subgraph_layers, 2, min_subgraph_layers_message);

// @brief Enable plugin messages
DEFINE_string(pin, "YES", infer_threads_pinning_message);

/// @brief Enables multiline text output instead of progress bar
DEFINE_bool(stream_output, false, stream_output_message);

/// @brief Enables saving a summary of the run
DEFINE_bool(save_run_summary, false, save_run_summary_message);

/// @brief Path to a folder where statistics report is stored
DEFINE_string(report_folder, "", report_folder_message);

/// @brief Define flag for showing progress bar <br>
DEFINE_bool(progress, false, progress_message);

/// @brief Path to a plugins_xml file
DEFINE_string(plugins, "", plugins_message);

/// @brief Deprecated argument for path to a plugins_xml file
DEFINE_string(plugins_xml_file, "", old_plugins_message);

/// @brief Path to a groundtruth file
DEFINE_string(groundtruth_loc, "", groundtruth_loc_message);

/// @brief Path to arch file
DEFINE_string(arch_file, "", arch_file_message);

/// @brief Path to arch file, same as arch_file
DEFINE_string(arch, "", arch_alias_message);

/// @brief Define flag for enable performance estimation
DEFINE_bool(perf_est, false, perf_est_message);

/// @brief Define flag whether the image is in bgr format
DEFINE_bool(bgr, false, bgr_message);

/// @brief Define flag for enable output results dumping
DEFINE_bool(dump_output, false, dump_output_message);

/// @brief Define flag for output directory where result files are dumped to
DEFINE_string(output_dir, "", output_dir_message);

/// @brief Define flag for enabling CSR dumping for AGX5 hostless designs.
DEFINE_bool(dump_csr, true, dump_csr_message);

/// Select folding options; 0,1,2,3
DEFINE_int32(folding_option, 1, folding_option_message);

/// @brief Define flag for enabling folding preprocessing
DEFINE_bool(fold_preprocessing, false, fold_preprocessing_message);

/// @brief encryption key
DEFINE_string(encryption_key, "", encryption_key_message);

/// @brief initialization vector
DEFINE_string(encryption_iv, "", encryption_iv_message);

/// @brief Define flag for enabling dump of debug network values
DEFINE_bool(debug_network, false, debug_network_message);

/// @brief encryption_key
DEFINE_bool(emulator_decryption, false, emulator_decryption_message);

/// @brief Flag for printing the hidden help message
DEFINE_bool(hidden_help, false, hidden_help_message);

/// @brief Whether Performance Estimator should calculate theoretical per-layer cycle counts. Internal use only. Must be
/// called with -perf_est.
DEFINE_bool(estimate_per_layer_latencies, false, estimate_per_layer_latencies_message);

/// @brief Show average precision in the report
DEFINE_bool(enable_object_detection_ap, false, enable_object_detection_ap_message);

/// @brief Let user specify the version of their YOLO graph.
DEFINE_string(yolo_version, "", yolo_version_message);

/// @brief Specify that the inputs should be read as binary.
DEFINE_bool(bin_data, false, bin_data_message);

/// @brief Report performance counters for the CPU subgraphs.
DEFINE_bool(pc, false, pc_message);

/// @brief Report performance counters for the CPU subgraphs and analysis sort hotpoint opts.
DEFINE_string(pcsort, "", pcsort_message);

/// @brief Define flag for using input image scale <br>
DEFINE_string(scale_values, "", input_image_scale_message);

/// @brief Define flag for using input image mean <br>
DEFINE_string(mean_values, "", input_image_mean_message);

/// @brief Define flag for using input image resize <br>
DEFINE_string(resize_type, "", input_image_resize_message);

/// @brief Enables early-access (EA) features of CoreDLA <br>
DEFINE_bool(enable_early_access, false, enable_early_access_message);

/// @brief Pass the name of the streaming input linux FIFO for use in the emulator model
DEFINE_string(streaming_input_pipe, "", "");

/// @brief Report the input feature reader, output feature writer, and filter reader memory access counts
DEFINE_bool(report_lsu_counters, false, report_lsu_counters_message);

/// @brief define flag dla benchmark verbosity
DEFINE_bool(verbose, false, verbose_message);

/// @brief maximum file size in MB that can be dumped to a txt. Used to avoid creating files that cannot be opened.
DEFINE_int32(max_output_file_size, 200, output_output_file_size_message);

/// @brief available DDR bandwidth in MB/s per DLA IP. Only matters for designs with DDR
DEFINE_int32(ddr_bw, -1, ddr_bandwidth_message);

/**
 * @brief Options that impact graph compile.
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowCompileOptions() {
  std::cout << std::endl << "Graph Compile Options:" << std::endl;
  std::cout << "    -folding_option                             " << folding_option_message << std::endl;
  std::cout << "    -fold_preprocessing                         " << fold_preprocessing_message << std::endl;
  std::cout << "    -min-subgraph-layers \"<integer>\"            " << min_subgraph_layers_message << std::endl;
}

/**
 * @brief Options that evaluate the correctness of the inference result.
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowAccuracyOptions() {
  std::cout << std::endl << "Accuracy Options:" << std::endl;
  std::cout << "    -dump_output                                " << dump_output_message << std::endl;
  std::cout << "    -groundtruth_loc                            " << groundtruth_loc_message << std::endl;
  std::cout << "    -enable_object_detection_ap                 " << enable_object_detection_ap_message << std::endl;
  std::cout << "    -yolo_version \"yolo-v3-tf/yolo-v3-tiny-tf\"  " << yolo_version_message << std::endl;
}

/**
 * @brief Shows options for statistic dumping, report dumping
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowStatsOrReportDumpingOptions() {
  std::cout << std::endl << "Statistics dumping options:" << std::endl;
  std::cout << "    -perf_est                                   " << perf_est_message << std::endl;
  std::cout << "    -ddr_bw                                     " << ddr_bandwidth_message << std::endl;
  std::cout << "    -progress                                   " << progress_message << std::endl;
  std::cout << "    -stream_output                              " << stream_output_message << std::endl;
  std::cout << "    -save_run_summary                           " << save_run_summary_message << std::endl;
  std::cout << "    -report_folder                              " << report_folder_message << std::endl;
  std::cout << "    -dump_csr                                   " << dump_csr_message << std::endl;
}

/**
 * @brief Shows preprocessing options for input data
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowPreprocessingOptions() {
  std::cout << std::endl << "Preprocessing Options:" << std::endl;
  std::cout << "    -bgr                                        " << bgr_message << std::endl;
  std::cout << "    -resize_type \"resize/pad_resize\"            " << input_image_resize_message << std::endl;
  std::cout << "    -scale_values                               " << input_image_scale_message << std::endl;
  std::cout << "    -mean_values                                " << input_image_mean_message << std::endl;
}

/**
 * @brief Shows help options for inference on the FPGA or any OpenVINO device.
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowInferenceOptions() {
  std::cout << std::endl << "Inference Options:" << std::endl;
  std::cout << "    -api \"<sync/async>\"                         " << api_message << std::endl;
  std::cout << "    -niter \"<integer>\"                          " << iterations_count_message << std::endl;
  std::cout << "    -nireq \"<integer>\"                          " << infer_requests_count_message << std::endl;
  std::cout << "    -b \"<integer>\"                              " << batch_size_message << std::endl;
  std::cout << "    -batch-size \"<integer>\"                     " << batch_size_alias_message << std::endl;
}

/**
 * @brief Shows help options for OpenVINO devices (CPU, GPU)
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowOpenVINODeviceOptions() {
  std::cout << std::endl << "CPU or GPU options:" << std::endl;
  std::cout << "    -nstreams \"<integer>\"                       " << infer_num_streams_message << std::endl;
  std::cout << "    -nthreads \"<integer>\"                       " << infer_num_threads_message << std::endl;
  std::cout << "    -pin \"YES/NO\"                               " << infer_threads_pinning_message << std::endl;
  std::cout << "    -l \"<absolute_path>\"                        " << custom_cpu_library_message << std::endl;
  std::cout << "    -pc                                           " << pc_message << std::endl;
  std::cout << "    -pcsort \"sort/no_sort/simple_sort\"          " << pcsort_message << std::endl;
}

/**
 * @brief This function prints a help message outlining options that are hidden from the user.
 * Options listed here should be experimental or features for internal use.
 * Please make sure your help text aligns with the other option in command line.
 */
static void PrintHiddenHelp() {
  std::cout << std::endl << "Hidden Options. Experimental, early access or internal options." << std::endl;
  std::cout << "    -enable_early_access              " << enable_early_access_message << std::endl;
  std::cout << "    -estimate_per_layer_latencies     " << estimate_per_layer_latencies_message << std::endl;
  std::cout << "    -debug_network                    " << debug_network_message << std::endl;
  std::cout << "    -max_output_file_size                    " << output_output_file_size_message << std::endl;
}

/**
 * @brief This function shows a help message. Add your new option in the appropriate section.
 * Please make sure your help text aligns with the other option in command line.
 */
static void ShowUsage() {
  std::cout << std::endl;
  std::cout << "dla_benchmark [OPTION]" << std::endl;
  std::cout << "Options:" << std::endl;
  std::cout << std::endl;
  std::cout << "    -h, --help                                  " << help_message << std::endl;
  std::cout << "    -m \"<path>\"                                 " << model_message << std::endl;
  std::cout << "    -network-file \"<path>\"                      " << network_file_alias_message << std::endl;
  std::cout << "    -cm \"<path>\"                                " << compiled_model_message << std::endl;
  std::cout << "    -d \"<device>\"                               " << target_device_message << std::endl;
  std::cout << "    -plugins                                    " << plugins_message << std::endl;
  std::cout << "    -plugins_xml_file                           " << old_plugins_message << std::endl;
  std::cout << "    -arch_file                                  " << arch_file_message << std::endl;
  std::cout << "    -arch                                       " << arch_alias_message << std::endl;
  std::cout << "    -i \"<path>\"                                 " << input_message << std::endl;
  std::cout << "    -bin_data                                   " << bin_data_message << std::endl;
  std::cout << "    -output_dir                                 " << output_dir_message << std::endl;
  std::cout << "    -encryption_key                             " << encryption_key_message << std::endl;
  std::cout << "    -encryption_iv                              " << encryption_iv_message << std::endl;
  std::cout << "    -emulator_decryption                        " << emulator_decryption_message << std::endl;
  std::cout << "    -verbose                                    " << verbose_message << std::endl;
  std::cout << "    -hidden_help                                " << hidden_help_message << std::endl;
  ShowInferenceOptions();
  ShowCompileOptions();
  ShowPreprocessingOptions();
  ShowAccuracyOptions();
  ShowStatsOrReportDumpingOptions();
  ShowOpenVINODeviceOptions();
}


// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "gflags/gflags.h"
#include "dla_plugin_config.hpp"

/// @brief message for help argument
static const char help_message[] = "Print a usage message.";

/// @brief message for model argument
static const char model_message[] = "Required. Path to an .xml file with a trained model.";

/// @brief message for images argument
static const char image_message[] =
    "Required. Path to a folder with images or path to an image files: a .ubyte file for LeNet"
    " and a .bmp file for the other networks.";

/// @brief message for assigning cnn calculation to device
static const char target_device_message[] =
    "Optional. Specify the target device to infer on (the list of available devices is shown below). "
    "Default value is CPU. Use \"-d HETERO:<comma_separated_devices_list>\" format to specify HETERO plugin. "
    "Sample will look for a suitable plugin for device specified.";

/// @brief message for plugin messages
static const char plugin_message[] = "Optional. Enables messages from a plugin";

// @brief message for performance counters option
static const char plugins_message[] = "Optional. Select a custom plugins_xml file to use.";
// @brief message for architecture .arch file
static const char arch_file_message[] = "Optional. Provide a path for the architecture .arch file.";

/// @brief Define flag for showing help message <br>
DEFINE_bool(h, false, help_message);

/// @brief Define parameter for set image file <br>
/// It is a required parameter
DEFINE_string(i, "", image_message);

/// @brief Define parameter for set model file <br>
/// It is a required parameter
DEFINE_string(m, "", model_message);

/// @brief device the target device to infer on <br>
/// It is an optional parameter
DEFINE_string(d, "CPU", target_device_message);

/// @brief Path to a plugins_xml file
DEFINE_string(plugins, "", plugins_message);
/// @brief Path to arch file
DEFINE_string(arch_file, "", arch_file_message);


/**
 * @brief This function show a help message
 */
static void show_usage() {
    std::cout << std::endl;
    std::cout << "classification_sample_async [OPTION]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << std::endl;
    std::cout << "    -h                      " << help_message << std::endl;
    std::cout << "    -m \"<path>\"             " << model_message << std::endl;
    std::cout << "    -i \"<path>\"             " << image_message << std::endl;
    std::cout << "    -d \"<device>\"           " << target_device_message << std::endl;
}

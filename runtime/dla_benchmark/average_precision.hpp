// Copyright (C) 2018-2023 Altera Corporation
// SPDX-License-Identifier: Apache-2.0
//
// Description: The file defines functions calculate mAP and COCO AP metrics. See average_precision.cpp for a
// detailed explaination.

#ifndef DLA_BENCHMARK_OBJECT_DETECTION_H_
#define DLA_BENCHMARK_OBJECT_DETECTION_H_

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

#include "openvino/openvino.hpp"

#undef UNICODE

// Indexes for raw bounding box.
#define BBOX_X 0
#define BBOX_Y 1
#define BBOX_W 2
#define BBOX_H 3
#define BBOX_CONFIDENCE 4

// Indices for input image shapes.
#define IMG_W 0
#define IMG_H 1

// Indices for parsed bounding boxes.
#define X_MAX 0
#define X_MIN 1
#define Y_MAX 2
#define Y_MIN 3

// Convenient aliases.
template <typename T>
using Box = std::vector<T>;

template <typename T>
using Tensor2d = std::vector<std::vector<T>>;

template <typename T>
using Tensor3d = std::vector<std::vector<std::vector<T>>>;

// A set of supported YOLO graphs and its variants.
static std::set<std::string> supported_yolo_versions = {"yolo-v3-tf", "yolo-v3-tiny-tf"};

// Each image will have a prediction entry containing coordinates,
// class scores of prediction boxes, predicted class, and size.
typedef struct prediction_entry_t {
  std::vector<double> x_max;
  std::vector<double> x_min;
  std::vector<double> y_max;
  std::vector<double> y_min;
  // scores for highest class
  std::vector<double> cls_score;
  // class with highest probability
  std::vector<int> cls;
  unsigned size;

  Box<double> box_at(unsigned idx) { return {x_max[idx], x_min[idx], y_max[idx], y_min[idx]}; }
} PredictionEntry;

// Each image will have an annotation entry containing coordinates and
// the true label specified in `cls`.
typedef struct annotation_entry_t {
  std::vector<double> x_max;
  std::vector<double> x_min;
  std::vector<double> y_max;
  std::vector<double> y_min;
  std::vector<int> cls;
  unsigned size;

  Box<double> box_at(unsigned idx) { return {x_max[idx], x_min[idx], y_max[idx], y_min[idx]}; }
} AnnotationEntry;

// Stores runtime variables.
static struct runtime_const_t {
  // Actually means number of validation image.
  unsigned niter;
  unsigned batch_size;
  std::string name;
  std::string groundtruth_loc;
  std::string input_loc;
  std::string report_folder;
  const std::string gt_extension = "txt";

  Tensor2d<std::string> input_image_path;
  Tensor2d<double> source_image_sizes;
} runtime_vars;

// Stores constants for evaluation.
static struct meta_t {
  // Filtering parameters,
  const double confidence_threshold = 0.001;
  const double iou_threshold = 0.5;

  // Parameters for parsing and resizing.
  const unsigned num_classes = 80;
  const unsigned channel = 255;
  const unsigned box_per_channel = 3;
  const unsigned pbox_size = 85;
  const std::vector<double> dst_image_size = {416, 416};

  // Dimensions of grid cells and anchor boxes.
  const std::map<std::string, std::map<unsigned, std::vector<double>>> anchor_sizes{
      {
          "yolo-v3-tiny-tf",
          {{13, {81, 82, 135, 169, 344, 319}}, {26, {23, 27, 37, 58, 81, 82}}},
      },
      {
          "yolo-v3-tf",
          {{13, {116, 90, 156, 198, 373, 326}}, {26, {30, 61, 62, 45, 59, 119}}, {52, {10, 13, 16, 30, 33, 23}}},
      }};
  const std::map<std::string, std::vector<unsigned>> grid_sizes = {
      {"yolo-v3-tiny-tf", {26, 13}},
      {"yolo-v3-tf", {13, 26, 52}},
  };

  // Use of `boundary` in IoU calculation.
  const int boundary = 1;

  // IoU threshold for metrics calculation.
  const double strict_metric = 0.75;
  const double pascal_voc_metric = 0.5;
  const std::vector<double> coco_metric = {0.5, 0.55, 0.6, 0.65, 0.7, 0.75, 0.8, 0.85, 0.9, 0.95};

  // AP calculation
  const unsigned ap_interval = 11;
  const unsigned coco_interval = 101;
} yolo_meta;

// Returns `true` if the given YOLO graph, `name`, is supported. Else, `false` is returned.
bool inline is_yolo_supported(std::string &name) {
  return (supported_yolo_versions.find(name) != supported_yolo_versions.end());
}

// Sets runtime variables.
void set_runtime(std::string name,
                 unsigned niter,
                 unsigned batch_size,
                 const std::string &input_loc,
                 const std::string &annotation_loc);

// Entry point of this subroutine.
std::pair<double, double> validate_yolo_wrapper(std::map<std::string, ov::TensorVector> &raw_results,
                                                const std::vector<ov::Output<const ov::Node>> &result_layout,
                                                std::vector<std::string> input_files);

#endif  // DLA_BENCHMARK__OBJECT_DETECTION_H_

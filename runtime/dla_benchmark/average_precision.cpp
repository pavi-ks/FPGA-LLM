// Copyright (C) 2018-2023 Altera Corporation
// SPDX-License-Identifier: Apache-2.0
//
// The function of this file is to provide mAP and COCO AP calculation in metrics_eval
// and metrics_update. The calculation is comprised with two parts, 1) data preprocessing,
// and 2) metrics calculation. Data preprocessing consists of prediction box parsing;
// resize and filtering; non-max suppression; and clipping. The preprocessed data is stored
// in `PredictionEntry` and `AnnotationEntry` structs, which are used in the `metrics_update`
// and `metrics_eval`. `metrics_update` updates intermidiate statistics to form the batched
// statistics, and the `metrics_eval` calculated the integral of the ROC of P-R curve. All of
// the metadata should be set in the header file and the runtime invariants are set using
// `set_runtime`. The validate_yolo_wrapper is the main entery point of the subroutine.
//
// The mAP algorithm is built according to the section 2.2 in https://arxiv.org/pdf/1607.03476.pdf
// and OpenVINO's accuracy_checker. The COCO AP algorithm is specified in
// https://cocodataset.org/#detection-eval. The result is compared value-by-value with the
// result from OpenVINO's accuracy_checker using dlsdk launcher. To obtain the the golden
// result, apply the steps in https://docs.openvino.ai/latest/omz_models_model_yolo_v3_tf.html.

#include "average_precision.hpp"
#include <cmath>
#if defined(_WIN32) || defined(_WIN64)
#include <io.h>
#else
#include <dirent.h>
#endif
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
#include <filesystem>
namespace fs = std::filesystem;
#endif
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <utility>
#include <sstream>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <samples/slog.hpp>
#include "utils.hpp"

#define VERBOSE 0

// Parses predicted boxes in `results_data` to a 2d tensor `raw_predictions`. The parameter
// `batch` indicates the image which corresponds to those predicted boxes.
// Order: conv2d_12[1x255x26x26] -> conv2d_9[1x255x13x13], NCHW order
void parse_prediction_boxes(std::vector<double> &predicted_val, Tensor2d<double> &raw_predictions) {
  raw_predictions.emplace_back(std::vector<double>{});
  const std::vector<unsigned> &grid_sizes = yolo_meta.grid_sizes.at(runtime_vars.name);

  int total_boxes{0};
  std::for_each(std::begin(grid_sizes), std::end(grid_sizes), [&](unsigned n) {
    total_boxes += std::pow(n, 2) * yolo_meta.box_per_channel;
  });

  for (int count = 0; count < total_boxes; count++) {
    raw_predictions.emplace_back(Box<double>{});
    raw_predictions[count].reserve(yolo_meta.pbox_size);
  }

  auto index_of = [=](int n, int c, int h, int w, int C, int H, int W) {
    return n * C * H * W + c * H * W + h * W + w;
  };

  // first are boxes in 26x26 grid
  // treat each tensor as 3 batchs
  for (int grid : grid_sizes) {
    // offset to where the data is retrieved
    int data_offset{0};
    // offset to where the data is inserted
    int position_offset{0};
    for (int n : grid_sizes) {
      if (n == grid) break;
      data_offset += pow(n, 2) * yolo_meta.channel;
      position_offset += pow(n, 2) * yolo_meta.box_per_channel;
    }

    int N = yolo_meta.box_per_channel, C = yolo_meta.pbox_size, H = grid, W = grid;

    for (int n = 0; n < N; n++) {
      for (int c = 0; c < C; c++) {
        for (int h = 0; h < H; h++) {
          for (int w = 0; w < W; w++) {
            // corresponds to #c data for grid #h,w, of the #n anchor
            Box<double> &pbox = raw_predictions[position_offset + n * H * W + h * W + w];
            // fills prediction boxes
            pbox.emplace_back(predicted_val[data_offset + index_of(n, c, h, w, C, H, W)]);
          }
        }
      }
    }
  }
}

// Parses annotation boxes stored in text file and stores in a 3d tensor `raw_annotation`.
// Precondition: the file is formatted such that each line contains 5 doubles and separated
// by spaces, i.e. [class, x, y, width, height]. Returns -3 if cannot read from file.
int parse_annotation_boxes(Tensor3d<double> &raw_annotation, const std::string &path) {
  int err = 0;
  std::ifstream annotation_file(path);
  if (!annotation_file.is_open()) {
    slog::err << "Couldn't access path: " << path << slog::endl;
    err = -3;
  } else {
    Tensor2d<double> annotation_box;
    int class_id;
    double x, y, w, h;
    while (annotation_file >> class_id >> x >> y >> w >> h) {
      annotation_box.emplace_back(Box<double>{x, y, w, h, (double)class_id});
    }
    raw_annotation.emplace_back(annotation_box);
  }
  return err;
}

// Extracts filenames in `path` with given extension specified in `extensions`.
// Returns the number of file with extension `ext`, or -1 for error.
int walk_dirent(std::vector<std::string> &names, const std::string &path, std::string ext) {
#if defined(_WIN32) || defined(_WIN64)
#if ((defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || __cplusplus >= 201703L)
  int count = 0;
  for (const auto &entry : fs::directory_iterator(path)) {
    if (fs::is_regular_file(entry)) {
      std::string filename = entry.path().filename().string();
      std::string file_extension = filename.substr(filename.find_last_of(".") + 1);
      if (file_extension == ext) {
        names.emplace_back(filename);
        count++;
      }
    }
  }
#endif
#else
  DIR *dir = opendir(path.c_str());
  int count = 0;
  if (!dir) {
    slog::err << "Couldn't access path: " << path << slog::endl;
    count = -1;
  } else {
    for (struct dirent *dent; (dent = readdir(dir)) != nullptr;) {
      std::string dirname(dent->d_name);
      std::string stem = GetStem(dirname);
      std::string extension = GetExtension(dirname);
      if (stem == "" || stem == "." || extension != ext) continue;
      names.emplace_back(stem);
      count += 1;
    }
    closedir(dir);
  }
#endif
  return count;
}

// Dispatches each step of collecting predicted boxes, annotation boxes, and shapes.
// The function returns 0 on success, -1 for mismatch in the number of annotation files
// and validation images, -2 for missing annotation file, -3 for failing to access annotation
// file, and -4 for failing to access validation image.
int collect_validation_dataset(std::vector<std::string> &image_paths,
                               Tensor3d<double> &raw_annotations,
                               Tensor2d<double> &shapes) {
  int err = 0;

  // set of annotation file name
  std::vector<std::string> tmp;
  int num_file = walk_dirent(tmp, runtime_vars.groundtruth_loc, runtime_vars.gt_extension);
  if (num_file < (int)(runtime_vars.batch_size * runtime_vars.niter)) {
    if (num_file >= 0) {
      slog::err << "Not enough validation data found. " << runtime_vars.batch_size * runtime_vars.niter << " required, "
                << num_file << " provided." << slog::endl;
    }
    err = -1;
  } else {
    std::set<std::string> annotation_file_index(tmp.begin(), tmp.end());

    // gets all images, corresponding annotation, and shapes
    std::string gt_path;
    for (unsigned batch = 0; batch < runtime_vars.batch_size * runtime_vars.niter; batch++) {
      std::string image_path(image_paths[batch]);
      std::string img_name = GetStem(image_path);
      if (annotation_file_index.find(img_name) == annotation_file_index.end()) {
        slog::err << "Missing annotation file for validation image: " << image_paths[batch] << slog::endl;
        err = -2;
        break;
      } else {
        gt_path = runtime_vars.groundtruth_loc + "/" + img_name + "." + runtime_vars.gt_extension;

        // gets image dimensions
        cv::Mat image = cv::imread(image_paths[batch]);
        if (image.data == nullptr || image.empty()) {
          slog::err << "Couldn't open input image: " << image_paths[batch] << slog::endl;
          err = -4;
          break;
        }

        err = parse_annotation_boxes(raw_annotations, gt_path);
        if (err != 0) break;
        shapes.emplace_back(Box<double>{(double)image.cols, (double)image.rows});
      }
    }
  }
  return err;
}

// Removes items at `indices` in the vector `vec`
template <typename T>
void reduce_by_index(std::vector<T> &vec, std::vector<unsigned> indices) {
  std::sort(indices.begin(), indices.end());
  for (auto it = indices.rbegin(); it != indices.rend(); it++) {
    vec.erase(vec.begin() + *it);
  }
}

// Calculates and returns the Intersection over Union score for two boxes by
// calculating their area of overlap and area of union.
double intersection_over_union(Box<double> box1, Box<double> box2) {
  using namespace std;
  {
    double intersect_length_x =
        max(0.0, min(box1[X_MAX], box2[X_MAX]) - max(box1[X_MIN], box2[X_MIN]) + yolo_meta.boundary);
    double intersect_length_y =
        max(0.0, min(box1[Y_MAX], box2[Y_MAX]) - max(box1[Y_MIN], box2[Y_MIN]) + yolo_meta.boundary);
    double intersection_of_area = intersect_length_x * intersect_length_y;
    double box1_area =
        (box1[X_MAX] - box1[X_MIN] + yolo_meta.boundary) * (box1[Y_MAX] - box1[Y_MIN] + yolo_meta.boundary);
    double box2_area =
        (box2[X_MAX] - box2[X_MIN] + yolo_meta.boundary) * (box2[Y_MAX] - box2[Y_MIN] + yolo_meta.boundary);
    double union_of_area = box1_area + box2_area - intersection_of_area;
    return (union_of_area > 0.0) ? intersection_of_area / union_of_area : 0.0;
  }  // namespace std
}

// This function returns the index of the largest element in the vector `vec`.
template <typename T>
int argmax(std::vector<T> vec) {
  return std::distance(vec.begin(), std::max_element(vec.begin(), vec.end()));
}

// This function returns the index of the largest element in the iterator from `begin` to `end`.
template <typename Iter>
int argmax(Iter begin, Iter end) {
  return std::distance(begin, std::max_element(begin, end));
}

// Resize the coordinates of bounding boxes from relative ratio to grid cell to the actual coordinates in pixel.
// This function resizes prediction boxes in the 2d tensor `raw_predictions` based on the definition in page 4 of
// https://arxiv.org/pdf/1612.08242.pdf. The prediction boxes are also filtered based on their confidence score
// and class specific score. The result is stored in an instance of `PredictionEntry` which is used for statistics
// calculation.
void resize_and_filter_prediction_boxes(Tensor2d<double> &raw_predictions,
                                        PredictionEntry &prediction,
                                        const unsigned batch) {
  unsigned size = 0;

#if VERBOSE == 1
  unsigned c12 = 0, c9 = 0, c58 = 0, c66 = 0, c74 = 0;
#endif

  for (unsigned grid : yolo_meta.grid_sizes.at(runtime_vars.name)) {
    unsigned offset{0};
    for (unsigned n : yolo_meta.grid_sizes.at(runtime_vars.name)) {
      if (n == grid) break;
      offset += pow(n, 2) * yolo_meta.box_per_channel;
    }
    for (unsigned x = 0; x < grid; x++) {
      for (unsigned y = 0; y < grid; y++) {
        for (unsigned n = 0; n < yolo_meta.box_per_channel; n++) {
          unsigned bbox_idx = offset + n * pow(grid, 2) + y * grid + x;
          Box<double> &bbox = raw_predictions[bbox_idx];

          // find the predicted label as the class with highest score
          int label = argmax(bbox.begin() + (yolo_meta.pbox_size - yolo_meta.num_classes), bbox.end());
          double cls_score = bbox[BBOX_CONFIDENCE] * bbox[(yolo_meta.pbox_size - yolo_meta.num_classes) + label];
          // filter outliers with low confidence score or class score
          if (bbox[BBOX_CONFIDENCE] < yolo_meta.confidence_threshold || cls_score < yolo_meta.confidence_threshold)
            continue;
          prediction.cls.push_back(label);
          prediction.cls_score.push_back(cls_score);
#if VERBOSE == 1
          c74 += (unsigned)(grid == 52);
          c66 += (unsigned)(grid == 26);
          c58 += (unsigned)(grid == 13);
          c12 += (unsigned)(grid == 26);
          c9 += (unsigned)(grid == 13);
#endif
          // deduce anchor box width and height
          unsigned dim = yolo_meta.anchor_sizes.at(runtime_vars.name).at(grid).size() / yolo_meta.box_per_channel;
          double anchor_w = yolo_meta.anchor_sizes.at(runtime_vars.name).at(grid)[n * dim];
          double anchor_h = yolo_meta.anchor_sizes.at(runtime_vars.name).at(grid)[n * dim + 1];

          // calculate width and height of bbox
          double bbox_center_x = (bbox[BBOX_X] + x) / grid;
          double bbox_center_y = (bbox[BBOX_Y] + y) / grid;
          double bbox_w = exp(bbox[BBOX_W]) * anchor_w / yolo_meta.dst_image_size[IMG_W];
          double bbox_h = exp(bbox[BBOX_H]) * anchor_h / yolo_meta.dst_image_size[IMG_H];

          // calculate actual coordinates of bbox
          double x_max, x_min, y_max, y_min;
          double w = runtime_vars.source_image_sizes[batch][IMG_W];
          double h = runtime_vars.source_image_sizes[batch][IMG_H];

          x_max = w * (bbox_center_x + bbox_w / 2.0);
          x_min = w * (bbox_center_x - bbox_w / 2.0);
          y_max = h * (bbox_center_y + bbox_h / 2.0);
          y_min = h * (bbox_center_y - bbox_h / 2.0);

          prediction.x_max.emplace_back(x_max);
          prediction.x_min.emplace_back(x_min);
          prediction.y_max.emplace_back(y_max);
          prediction.y_min.emplace_back(y_min);

          size += 1;
        }
      }
    }
  }
  prediction.size = size;
#if VERBOSE == 1
  if (runtime_vars.name == "yolo-v3-tf") {
    slog::info << "prediction boxes from conv2d58: " << c58 << slog::endl;
    slog::info << "prediction boxes from conv2d66: " << c66 << slog::endl;
    slog::info << "prediction boxes from conv2d74: " << c74 << slog::endl;
  } else if (runtime_vars.name == "yolo-v3-tiny-tf") {
    slog::info << "prediction boxes from conv2d12: " << c12 << slog::endl;
    slog::info << "prediction boxes from conv2d9: " << c9 << slog::endl;
  }
#endif
}

// Returns indices of `vec` sorted in descending order.
std::vector<unsigned> argsort_gt(const std::vector<double> &vec) {
  std::vector<unsigned> order(vec.size());
  std::generate(order.begin(), order.end(), [n = 0]() mutable { return n++; });
  std::sort(order.begin(), order.end(), [&](int i1, int i2) { return vec[i1] > vec[i2]; });
  return order;
}

// Performs non-maximum suppression algorithm to eliminate repetitive bounding boxes.
// A bounding box is preserved iff. it has the highest confidence score over all
// overlapping bounding boxes.
void nms(PredictionEntry &prediction) {
  if (prediction.size == 0) return;
  std::vector<unsigned> &&order = argsort_gt(prediction.cls_score);
  std::vector<unsigned> keep;
  std::set<unsigned> discard;
  unsigned top_score_idx;

  while (discard.size() < order.size()) {
    bool has_top = false;
    for (unsigned idx : order) {
      if (discard.find(idx) != discard.end()) continue;
      if (!has_top) {
        has_top = true;
        top_score_idx = idx;
        keep.emplace_back(top_score_idx);
        discard.insert(top_score_idx);
        continue;
      }
      double iou = intersection_over_union(prediction.box_at(idx), prediction.box_at(top_score_idx));
      if (iou > yolo_meta.iou_threshold) {
        discard.insert(idx);
      }
    }
  }

  std::vector<unsigned> discard_idx(discard.size());
  std::vector<unsigned> indexes(discard.begin(), discard.end());
  std::sort(indexes.begin(), indexes.end());
  std::sort(keep.begin(), keep.end());
  std::vector<unsigned>::iterator it =
      std::set_difference(indexes.begin(), indexes.end(), keep.begin(), keep.end(), discard_idx.begin());
  discard_idx.resize(it - discard_idx.begin());

  // remove filtered predicted bounding boxes.
  reduce_by_index(prediction.x_max, discard_idx);
  reduce_by_index(prediction.x_min, discard_idx);
  reduce_by_index(prediction.y_max, discard_idx);
  reduce_by_index(prediction.y_min, discard_idx);
  reduce_by_index(prediction.cls_score, discard_idx);
  reduce_by_index(prediction.cls, discard_idx);
  prediction.size -= discard_idx.size();
}

// Calculates the actual size of the groundtruth bounding boxes.
void resize_annotation_boxes(Tensor3d<double> &raw_annotations, AnnotationEntry &annotation, const unsigned batch) {
  for (auto &gt_box : raw_annotations[batch]) {
    annotation.x_max.emplace_back(gt_box[BBOX_X] + gt_box[BBOX_W]);
    annotation.x_min.emplace_back(gt_box[BBOX_X]);
    annotation.y_max.emplace_back(gt_box[BBOX_Y] + gt_box[BBOX_H]);
    annotation.y_min.emplace_back(gt_box[BBOX_Y]);
    annotation.cls.emplace_back(gt_box[BBOX_CONFIDENCE]);
  }
  annotation.size = raw_annotations[batch].size();
}

// Limits the coordinates of predicted bounding boxes within the dimension of source image.
void clip_box(PredictionEntry &prediction, const unsigned batch) {
  if (prediction.size == 0) return;
  double x_upper_bound = runtime_vars.source_image_sizes[batch][IMG_W];
  double y_upper_bound = runtime_vars.source_image_sizes[batch][IMG_H];
  auto _clip = [](double v, double lower, double upper) {  return (v < lower) ? lower : ((v > upper) ? upper : v); };
  for (unsigned idx = 0; idx < prediction.size; idx++) {
    prediction.x_max[idx] = _clip(prediction.x_max[idx], 0, x_upper_bound);
    prediction.x_min[idx] = _clip(prediction.x_min[idx], 0, x_upper_bound);
    prediction.y_max[idx] = _clip(prediction.y_max[idx], 0, y_upper_bound);
    prediction.y_min[idx] = _clip(prediction.y_min[idx], 0, y_upper_bound);
  }
}

// Calculates area under the PR curve using 11-intervaled sum.
double average_precision(const std::vector<double> &precision, const std::vector<double> &recall, unsigned interval) {
  double result = 0.0;
  double step = 1 / (double)(interval - 1);
  for (unsigned intvl = 0; intvl < interval; intvl++) {
    double point = step * intvl;
    double max_precision = 0.0;
    for (unsigned idx = 0; idx < recall.size(); idx++) {
      if (recall[idx] >= point) {
        if (precision[idx] > max_precision) {
          max_precision = precision[idx];
        }
      }
    }
    result += max_precision / (double)interval;
  }
  return result;
}

// Stores intermediate statistics for AP calculation. AP's are calculated from
// true-positive, false-positive, and the number of targets, sorted
// by the class score of the predicted bounding box.
typedef struct _map_stats {
  int num_gt_object;
  std::vector<double> scores;
  std::vector<int> true_positive;
  std::vector<int> false_positive;

  _map_stats() { this->num_gt_object = 0; }
} mAPStats;

// Calculates the 11-point interpolated mAP.
std::vector<mAPStats> mean_average_precision(PredictionEntry &prediction, AnnotationEntry &annotation, double thresh) {
  std::vector<int> class_list(yolo_meta.num_classes);
  std::generate(class_list.begin(), class_list.end(), [n = 0]() mutable { return n++; });

  std::vector<mAPStats> image_result(yolo_meta.num_classes, mAPStats{});

  // average precision for each class
  for (int category : class_list) {
    // total number of bounding boxes in the annotation.
    int num_gt_object =
        std::count_if(annotation.cls.begin(), annotation.cls.end(), [&](int &cls) { return (cls == (int)category); });

    // total number of predicted bounding boxes.
    int num_pred_boxes =
        std::count_if(prediction.cls.begin(), prediction.cls.end(), [&](int &cls) { return (cls == (int)category); });

    image_result[category].num_gt_object = num_gt_object;

    // stores the scores for sorting out the correct order of TP and FP.
    image_result[category].true_positive.resize(num_pred_boxes, 0);
    image_result[category].false_positive.resize(num_pred_boxes, 0);
    image_result[category].scores.resize(num_pred_boxes, 0.0);
    std::set<unsigned> matched_gtbox;

    unsigned pred_num = 0;
    std::vector<unsigned> &&sorted_pbox_idx = argsort_gt(prediction.cls_score);
    for (unsigned &pbox_idx : sorted_pbox_idx) {
      if (prediction.cls[pbox_idx] != category) continue;
      image_result[category].scores[pred_num] = prediction.cls_score[pbox_idx];

      unsigned most_overlapped_idx = 0;
      double most_overlapped_iou = 0.0;

      // finds the most overlapped predicted bounding box.
      for (unsigned gtbox_idx = 0; gtbox_idx < annotation.size; gtbox_idx++) {
        if (annotation.cls[gtbox_idx] != category) continue;
        double iou = intersection_over_union(prediction.box_at(pbox_idx), annotation.box_at(gtbox_idx));
        if (iou > most_overlapped_iou) {
          most_overlapped_iou = iou;
          most_overlapped_idx = gtbox_idx;
        }
      }
      // when there is no ground truth, all predicted boxes are false positive,
      // and they are preserved for batched AP calculation.
      if (!num_gt_object) {
        image_result[category].false_positive[pred_num++] = 1;
      } else {
        // the predicted bounding box is a true positive iff. it is the most overlapped,
        // the matched groundtruth bounding box has not been matched previously, and
        // the iou is above `thresh`.
        if (most_overlapped_iou >= thresh) {
          if (matched_gtbox.find(most_overlapped_idx) == matched_gtbox.end()) {
            matched_gtbox.insert(most_overlapped_idx);
            image_result[category].true_positive[pred_num++] = 1;
          } else {
            image_result[category].false_positive[pred_num++] = 1;
          }
        } else {
          image_result[category].false_positive[pred_num++] = 1;
        }
      }
    }
  }
  return image_result;
}

// Initializes runtime variables in `runtime_vars` struct.
void set_runtime(std::string name,
                 unsigned niter,
                 unsigned batch_size,
                 const std::string &input_loc,
                 const std::string &annotation_loc) {
  runtime_vars.name = name;
  runtime_vars.niter = niter;
  runtime_vars.batch_size = batch_size;
  runtime_vars.groundtruth_loc = annotation_loc;
  runtime_vars.input_loc = input_loc;
}

// Return type of function `validate_yolo`.
struct metrics {
  std::vector<mAPStats> map;
  Tensor2d<mAPStats> coco;
};

// Main function that takes the results data and annotation location, and calculates mAP score for the network.
struct metrics validate_yolo(std::vector<double> &results_data,
                             Tensor3d<double> &raw_annotations,
                             const unsigned batch) {
  Tensor2d<double> raw_predictions;
  PredictionEntry prediction;
  AnnotationEntry annotation;

  // executes accuracy check recipes.
  try {
    parse_prediction_boxes(results_data, raw_predictions);
    resize_and_filter_prediction_boxes(raw_predictions, prediction, batch);
    resize_annotation_boxes(raw_annotations, annotation, batch);
    nms(prediction);
    clip_box(prediction, batch);
  } catch (const std::exception &e) {
    slog::err << "Abort postprocessing." << slog::endl;
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }

  // mAP
  std::vector<mAPStats> map_stats = mean_average_precision(prediction, annotation, yolo_meta.pascal_voc_metric);

  // COCO metric
  Tensor2d<mAPStats> coco_ap_stats;
  std::for_each(std::begin(yolo_meta.coco_metric), std::end(yolo_meta.coco_metric), [&](const double thresh) {
    coco_ap_stats.emplace_back(mean_average_precision(prediction, annotation, thresh));
  });

  return {map_stats, coco_ap_stats};
}

// This function appends all of the elements in `v2` at the end of `v1` in order.
template <typename T>
void extend(std::vector<T> &v1, const std::vector<T> &v2) {
  v1.reserve(v1.size() + v2.size());
  v1.insert(v1.end(), v2.begin(), v2.end());
}

// Updates the batched statistics from individual image's result. The final batched AP and COCO AP is
// calculated based on updated `batched_stats`.
void metrics_update(std::vector<mAPStats> &batched_stats, const std::vector<mAPStats> &img_stats) {
  for (unsigned cat = 0; cat < yolo_meta.num_classes; cat++) {
    batched_stats[cat].num_gt_object += img_stats[cat].num_gt_object;
    // updates batched statistics. omits the class where no prediction presents.
    if (!img_stats[cat].scores.size()) continue;
    extend(batched_stats[cat].scores, img_stats[cat].scores);
    extend(batched_stats[cat].true_positive, img_stats[cat].true_positive);
    extend(batched_stats[cat].false_positive, img_stats[cat].false_positive);
  }
}

// Calculates AP using the given integral function.
double metrics_eval(const std::vector<mAPStats> &stats, unsigned interval) {
  std::vector<double> class_aps;
  for (unsigned category = 0; category < yolo_meta.num_classes; category++) {
    // omits the class when there is no prediction presents.
    if (!stats[category].scores.size()) continue;
    // the predictions are false-positive when there is no groundtruth for this
    // class, and therefore the class AP is 0.0
    if (stats[category].num_gt_object == 0 && stats[category].scores.size()) {
      class_aps.push_back(0.0);
      continue;
    }

    int TP = 0, FP = 0;
    std::vector<double> precision, recall;

    // sorts the tp and fp based on the order of confidence score.
    std::vector<unsigned> &&sorted_stats_index = argsort_gt(stats[category].scores);
    // calculates intermediate statistics calculation.
    for (unsigned idx : sorted_stats_index) {
      TP += stats[category].true_positive[idx];
      FP += stats[category].false_positive[idx];
      precision.emplace_back(TP / (double)(TP + FP));
      recall.emplace_back(TP / (double)stats[category].num_gt_object);
    }
    // returns ROC of P-R curve.
    class_aps.emplace_back(average_precision(precision, recall, interval));
  }
  return std::accumulate(class_aps.begin(), class_aps.end(), 0.0) / (double)class_aps.size();
}

// Wrapper of the function `validate_yolo`. This function prepares data and dispatches metrics calculations for each
// validation image, accumulates metrics results, and returns the batched mAP and COCO AP.
std::pair<double, double> validate_yolo_wrapper(std::map<std::string, ov::TensorVector> &raw_results,
                                                const std::vector<ov::Output<const ov::Node>> &result_layout,
                                                std::vector<std::string> input_files) {
  slog::info << "Start validating yolo." << slog::endl;
  std::ofstream fout;
  fout.open("ap_report.txt");
  // preserves all correct paths to validation images.
  int num_image = runtime_vars.niter * runtime_vars.batch_size;
  std::vector<std::string> input_image_paths;
  std::sort(std::begin(input_files), std::end(input_files));
  // input_files is guaranteed not to be empty since that case is filtered out.
  for (auto &path : input_files) {
    if (path == "") break;
    if (num_image == 0) break;
    input_image_paths.push_back(path);
    num_image--;
  }

  // checks if there exists enough image files; this should always pass unless the image file is
  // deleted right after the inferencing step.
  if (num_image != 0) {
    slog::err << "Not enough image input found. " << runtime_vars.batch_size * runtime_vars.niter << " required, "
              << (runtime_vars.batch_size * runtime_vars.niter - num_image) << " provided." << slog::endl;
    exit(EXIT_FAILURE);
  }
  // stores all annotation boxes for each image from groundtruth file.
  // if an input image does not have a corresponding groundtruth file, an error occurs.
  Tensor3d<double> raw_annotations;
  int err = collect_validation_dataset(input_image_paths, raw_annotations, runtime_vars.source_image_sizes);
  if (err) exit(EXIT_FAILURE);

  // updates the metrics each image at a time to reduce memory overhead. the result for each image
  // is accumulated in `batched_stats` and it will be used for batched mAP and COCO AP calculation.
  metrics batched_stats;
  batched_stats.map.resize(yolo_meta.num_classes, mAPStats{});
  batched_stats.coco.resize(yolo_meta.coco_metric.size(), std::vector<mAPStats>{});
  std::for_each(batched_stats.coco.begin(), batched_stats.coco.end(), [&](std::vector<mAPStats> &stats) {
    stats.resize(yolo_meta.num_classes, mAPStats{});
  });

  for (unsigned batch = 0; batch < runtime_vars.niter; batch++) {
    for (unsigned img = 0; img < runtime_vars.batch_size; img++) {
      // stores the flattened output tensors from the resulting convolution layers.
      std::vector<double> curr_img_data;
      for (auto &item : result_layout) {
        const std::string &name = item.get_any_name();
        auto curr_outputBlob = raw_results.at(name).at(batch);
        auto output_tensor_start = curr_outputBlob.data<float>();
        unsigned output_size = curr_outputBlob.get_size() / runtime_vars.batch_size;
        unsigned offset = img * output_size;
        for (unsigned idx = 0; idx < output_size; idx++) {
          curr_img_data.push_back(output_tensor_start[idx + offset]);
        }
      }

      struct metrics &&curr_img_stats =
          validate_yolo(curr_img_data, raw_annotations, img + batch * runtime_vars.batch_size);
      metrics_update(batched_stats.map, curr_img_stats.map);
      for (unsigned thresh = 0; thresh < yolo_meta.coco_metric.size(); thresh++) {
        metrics_update(batched_stats.coco[thresh], curr_img_stats.coco[thresh]);
      }

      double img_AP = metrics_eval(curr_img_stats.map, yolo_meta.ap_interval);
      // fout << "image " << input_files[img] << " AP @ 0.5" << std::endl;
      fout << std::fixed << std::setprecision(10) << img_AP << std::endl;
    }
  }

  double map = metrics_eval(batched_stats.map, yolo_meta.ap_interval);
  double coco_ap = 0.0;
  for (auto &coco_stats : batched_stats.coco) {
    coco_ap += metrics_eval(coco_stats, yolo_meta.coco_interval);
  }
  coco_ap /= (double)yolo_meta.coco_metric.size();

  fout << "\nAP at IoU=.50: " << std::fixed << std::setprecision(6) << map * 100 << "%" << std::endl;
  fout << "AP at IoU=.50:.05:.95: " << std::fixed << std::setprecision(10) << coco_ap * 100 << "%" << std::endl;
  fout.close();

  std::cout << "ap_report.txt has been generated in the current directory." << std::endl;

  return std::make_pair(map, coco_ap);
}

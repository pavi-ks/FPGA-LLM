// Copyright (C) 2018-2023 Altera Corporation
// SPDX-License-Identifier: Apache-2.0
//
// Description: Wrappers for single inference requests and queues of inference requests.
//              Largely based off OpenVino's benchmark_app/infer_request_wrap.hpp
//              [openvinotoolkit/openvino › samples/cpp/benchmark_app/infer_request_wrap.hpp]
//              Note: Not all functions of ov::InferRequest is wrapped. More functions can be added.

#pragma once

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include <algorithm>

#include <openvino/openvino.hpp>
#include "statistics_report.hpp"
#include "utils.hpp"

typedef std::function<void(size_t id, const double latency, const std::exception_ptr& ptr)> QueueCallbackFunction;

// Wrapper class for ov::InferRequest. Handles asynchronous callbacks
class InferReqWrap final {
 public:
  using Ptr = std::shared_ptr<InferReqWrap>;

  ~InferReqWrap() = default;

  explicit InferReqWrap(ov::CompiledModel& model, size_t id, QueueCallbackFunction callbackQueue)
      : _request(model.create_infer_request()), _id(id), _callbackQueue(callbackQueue) {
    _request.set_callback([&](const std::exception_ptr& ptr) {
      _endTime = Time::now();
      _callbackQueue(_id, get_execution_time_in_milliseconds(), ptr);
    });
  }

  void start_async() {
    _startTime = Time::now();
    _request.start_async();
  }

  void wait() { _request.wait(); }

  void infer() {
    _startTime = Time::now();
    _request.infer();
    _endTime = Time::now();
    _callbackQueue(_id, get_execution_time_in_milliseconds(), nullptr);
  }

  std::vector<ov::ProfilingInfo> get_performance_counts() { return _request.get_profiling_info(); }

  ov::Tensor get_tensor(const std::string& name) { return _request.get_tensor(name); }

  double get_execution_time_in_milliseconds() const {
    auto execTime = std::chrono::duration_cast<ns>(_endTime - _startTime);
    return static_cast<double>(execTime.count()) * 0.000001;
  }

  void set_tensor(const std::string& name, const ov::Tensor& data) { _request.set_tensor(name, data); }

  void set_tensor(const ov::Output<const ov::Node>& port, const ov::Tensor& data) { _request.set_tensor(port, data); }

  ov::Tensor get_output_tensor() { return _request.get_output_tensor(); }

 private:
  ov::InferRequest _request;
  Time::time_point _startTime;
  Time::time_point _endTime;
  size_t _id;
  QueueCallbackFunction _callbackQueue;
};

// Handles a queue of inference requests.
class InferRequestsQueue final {
 public:
  InferRequestsQueue(ov::CompiledModel& model, size_t nireq) {
    for (size_t id = 0; id < nireq; id++) {
      requests.push_back(std::make_shared<InferReqWrap>(model,
                                                        id,
                                                        std::bind(&InferRequestsQueue::put_idle_request,
                                                                  this,
                                                                  std::placeholders::_1,
                                                                  std::placeholders::_2,
                                                                  std::placeholders::_3)));
      _idleIds.push(id);
    }
    reset_times();
  }

  ~InferRequestsQueue() {
    // Inference Request guarantee that it will wait for all asynchronous internal tasks in destructor
    // So it should be released before any context that the request can use inside internal asynchronous tasks
    // For example all members of InferRequestsQueue would be destroyed before `requests` vector
    // So requests can try to use this members from `put_idle_request()` that would be called from request callback
    // To avoid this we should move this vector declaration after all members declaration or just clear it manually in
    // destructor
    requests.clear();
  }

  void reset_times() {
    _startTime = Time::time_point::max();
    _endTime = Time::time_point::min();
    _latencies.clear();
  }

  double get_durations_in_milliseconds() {
    return std::chrono::duration_cast<ns>(_endTime - _startTime).count() * 0.000001;
  }

  void put_idle_request(size_t id, const double latency, const std::exception_ptr& ptr = nullptr) {
    std::unique_lock<std::mutex> lock(_mutex);
    if (ptr) {
      inferenceException = ptr;
    } else {
      _latencies.push_back(latency);
      _idleIds.push(id);
      _endTime = std::max(Time::now(), _endTime);
    }
    _cv.notify_one();
  }

  InferReqWrap::Ptr get_idle_request() {
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] {
      if (inferenceException) {
        std::rethrow_exception(inferenceException);
      }
      return _idleIds.size() > 0;
    });
    auto request = requests.at(_idleIds.front());
    _idleIds.pop();
    _startTime = std::min(Time::now(), _startTime);
    return request;
  }

  void wait_all() {
    std::unique_lock<std::mutex> lock(_mutex);
    _cv.wait(lock, [this] {
      if (inferenceException) {
        std::rethrow_exception(inferenceException);
      }
      return _idleIds.size() == requests.size();
    });
  }

  std::vector<double>& get_latencies() { return _latencies; }

  Time::time_point get_start_time() { return _startTime; }

  Time::time_point get_end_time() { return _endTime; }

  std::vector<InferReqWrap::Ptr> requests;

 private:
  std::queue<size_t> _idleIds;
  std::mutex _mutex;
  std::condition_variable _cv;
  Time::time_point _startTime;
  Time::time_point _endTime;
  std::vector<double> _latencies;
  std::exception_ptr inferenceException = nullptr;
};

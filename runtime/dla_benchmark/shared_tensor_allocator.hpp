// Copyright (C) 2018-2023 Altera Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include <algorithm>
#include "openvino/runtime/allocator.hpp"

// Modified from SharedTensorAllocator in [openvinotoolkit/openvino ›
// samples/cpp/benchmark_app/shared_tensor_allocator.hpp]
class SharedTensorAllocator {
 public:
  SharedTensorAllocator(size_t sizeBytes) : size(sizeBytes) { data = new char[size]; }

  // Copy Constructor
  SharedTensorAllocator(const SharedTensorAllocator& other) : size(other.size) {
    data = new char[size];
    std::copy(other.data, other.data + size, data);
  }

  // Copy Assignment Operator
  SharedTensorAllocator& operator=(const SharedTensorAllocator& other) {
    if (this != &other) {
      size = other.size;
      delete[] data;
      data = new char[size];
      std::copy(other.data, other.data + size, data);
    }
    return *this;
  }

  ~SharedTensorAllocator() { delete[] data; }

  char* get_buffer() { return data; }

  void* allocate(size_t bytes, const size_t) {
    return bytes <= this->size ? (void*)data : nullptr;
  }
  void deallocate(void* handle, const size_t bytes, const size_t) {
        if (handle == data) {
            delete[] data;
            data = nullptr;
        }
    }
  bool is_equal(const SharedTensorAllocator& other) const noexcept {return this == &other;}

 private:
  char* data;
  size_t size;
};

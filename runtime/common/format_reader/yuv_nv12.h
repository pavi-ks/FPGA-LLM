// Copyright (C) 2018-2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * \brief YUV NV12 reader
 * \file yuv_nv12.h
 */
#pragma once

#include <memory>
#include <string>

// clang-format off
#include "format_reader.h"
#include "register.h"
// clang-format on

namespace FormatReader {
/**
 * \class YUV_NV12
 * \brief Reader for YUV NV12 files
 */
class YUV_NV12 : public Reader {
private:
    static Register<YUV_NV12> reg;
    size_t _size = 0;

public:
    /**
     * \brief Constructor of YUV NV12 reader
     * @param filename - path to input data
     * @return YUV_NV12 reader object
     */
    explicit YUV_NV12(const std::string& filename);
    virtual ~YUV_NV12() {}

    /**
     * \brief Get size
     * @return size
     */
    size_t size() const override {
        return _size;
    }

    // langsu: ResizeType is a added by us to support custom resizing functions (only in opencv_wrapper).
    // format_reader is copied from openvino samples/cpp/common/format_reader/
    // this might need special care when doing a OV uplift
    std::shared_ptr<unsigned char> getData(size_t width, size_t height, Reader::ResizeType resize_type) override {
        if ((width * height * 3 / 2 != size())) {
            std::cout << "Image dimensions not match with NV12 file size \n";
            return nullptr;
        }
        return _data;
    }
};
}  // namespace FormatReader

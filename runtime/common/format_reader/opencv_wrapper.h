// Copyright (C) 2018-2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * \brief Image reader
 * \file opencv_wrapper.h
 */
#pragma once

#ifdef USE_OPENCV
#    include <memory>
#    include <string>

// clang-format off
#    include <opencv2/opencv.hpp>

#    include "format_reader.h"
#    include "register.h"
// clang-format on

namespace FormatReader {
/**
 * \class OCVMAT
 * \brief OpenCV Wrapper
 */
class OCVReader : public Reader {
private:
    cv::Mat img;
    size_t _size;
    static Register<OCVReader> reg;

public:
    /**
     * \brief Constructor of BMP reader
     * @param filename - path to input data
     * @return BitMap reader object
     */
    explicit OCVReader(const std::string& filename);
    virtual ~OCVReader() {}

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
    std::shared_ptr<unsigned char> getData(size_t width,
                                           size_t height,
                                           ResizeType resize_type) override;
};
}  // namespace FormatReader
#endif

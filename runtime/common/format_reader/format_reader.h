// Copyright (C) 2018-2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

/**
 * \brief Format reader abstract class implementation
 * \file format_reader.h
 */
#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#    ifdef IMPLEMENT_FORMAT_READER
#        define FORMAT_READER_API(type) extern "C" __declspec(dllexport) type
#    else
#        define FORMAT_READER_API(type) extern "C" type
#    endif
#elif (__GNUC__ >= 4)
#    ifdef IMPLEMENT_FORMAT_READER
#        define FORMAT_READER_API(type) extern "C" __attribute__((visibility("default"))) type
#    else
#        define FORMAT_READER_API(type) extern "C" type
#    endif
#else
#    define FORMAT_READER_API(TYPE) extern "C" TYPE
#endif

namespace FormatReader {
/**
 * \class FormatReader
 * \brief This is an abstract class for reading input data
 */
class Reader {
protected:
    /// \brief height
    size_t _height = 0;
    /// \brief width
    size_t _width = 0;
    /// \brief data
    std::shared_ptr<unsigned char> _data;
    /// \brief shape - data shape
    std::vector<size_t> _shape;

public:
    virtual ~Reader() = default;

    // langsu: ResizeType is a added by us to support custom resizing functions (only in opencv_wrapper).
    // format_reader is copied from openvino samples/cpp/common/format_reader/
    // this might need special care when doing a OV uplift
    enum ResizeType {
        RESIZE,  // resize the image to target (height, width)
        PAD_RESIZE,  // pad the image into a squared image and then resize the image to target (height, width)
    };

    /**
     * \brief Get width
     * @return width
     */
    size_t width() const {
        return _width;
    }

    /**
     * \brief Get height
     * @return height
     */
    size_t height() const {
        return _height;
    }

    /**
     * \brief Get full shape vector
     * @return vector of size_t values determining data shape
     */
    std::vector<size_t> shape() const {
        return _shape;
    }

    /**
     * \brief Get input data ptr
     * @return shared pointer with input data
     * @In case of using OpenCV, parameters width and height will be used for image resizing
     */
    // langsu: ResizeType is a added by us to support custom resizing functions (only in opencv_wrapper).
    // Needs special care when doing a OV uplift
    virtual std::shared_ptr<unsigned char> getData(size_t width = 0, size_t height = 0,
                                                   ResizeType resize_type = ResizeType::RESIZE) = 0;

    /**
     * \brief Get size
     * @return size
     */
    virtual size_t size() const = 0;
};

/**
 * \brief Function for create reader
 * @return Reader pointer
 */
FORMAT_READER_API(FormatReader::Reader*) CreateFormatReader(const char* filename);

}  // namespace FormatReader

#include "cplus_alg/data_adapters/cv_mat_adapter.h"

#include <cstring>
#include <stdexcept>

namespace cplus_alg {

namespace {

std::string cv_depth_to_dtype(int depth) {
    switch (depth) {
        case CV_8U: return "uint8";
        case CV_8S: return "int8";
        case CV_16U: return "uint16";
        case CV_16S: return "int16";
        case CV_32S: return "int32";
        case CV_32F: return "float32";
        case CV_64F: return "float64";
        default: throw std::runtime_error("unsupported cv::Mat depth");
    }
}

int dtype_to_cv_depth(const std::string& dtype) {
    if (dtype == "uint8") return CV_8U;
    if (dtype == "int8") return CV_8S;
    if (dtype == "uint16") return CV_16U;
    if (dtype == "int16") return CV_16S;
    if (dtype == "int32") return CV_32S;
    if (dtype == "float32") return CV_32F;
    if (dtype == "float64") return CV_64F;
    throw std::runtime_error("unsupported dtype: " + dtype);
}

} // namespace

data_buffer from_cv_mat(const cv::Mat& mat) {
    data_buffer buf;
    buf.shape = {mat.rows, mat.cols, mat.channels()};
    buf.dtype = cv_depth_to_dtype(mat.depth());
    buf.data = const_cast<void*>(static_cast<const void*>(mat.data));
    buf.size_bytes = mat.total() * mat.elemSize();
    return buf;
}

cv::Mat to_cv_mat(const data_buffer& buf) {
    if (buf.shape.size() != 2 && buf.shape.size() != 3) {
        throw std::runtime_error("data_buffer shape must be 2D or 3D for cv::Mat");
    }

    int rows = buf.shape[0];
    int cols = buf.shape[1];
    int channels = (buf.shape.size() == 3) ? buf.shape[2] : 1;

    int cv_type = CV_MAKETYPE(dtype_to_cv_depth(buf.dtype), channels);
    return cv::Mat(rows, cols, cv_type, buf.data);
}

} // namespace cplus_alg

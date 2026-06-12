#ifndef CPLUS_ALG_CV_MAT_ADAPTER_H
#define CPLUS_ALG_CV_MAT_ADAPTER_H

#include "cplus_alg/data_buffer.h"

#include <opencv2/core.hpp>

namespace cplus_alg {

// cv::Mat 转通用 data_buffer（非占有式视图）
data_buffer from_cv_mat(const cv::Mat& mat);

// data_buffer 转 cv::Mat（非占有式视图）
cv::Mat to_cv_mat(const data_buffer& buf);

} // namespace cplus_alg

#endif // CPLUS_ALG_CV_MAT_ADAPTER_H

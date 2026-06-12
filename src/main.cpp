#include "cplus_alg/alg_interface.h"
#include "cplus_alg/data_adapters/cv_mat_adapter.h"
#include "cplus_alg/logger.h"

#include <opencv2/opencv.hpp>

namespace alg = cplus_alg;

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        // 构造合成图像：100x100 灰度图，在 (20, 30) 处放一个 10x10 的白色方块
        cv::Mat image(100, 100, CV_8UC1, cv::Scalar(0));
        cv::rectangle(image, cv::Point(20, 30), cv::Point(29, 39), cv::Scalar(255), cv::FILLED);

        // 模板：10x10 白色方块
        cv::Mat templ(10, 10, CV_8UC1, cv::Scalar(255));

        alg::call_params params;
        params.set("method", std::string("ccorr_normed"))
              .set_buffer("template", alg::from_cv_mat(templ));

        auto result = alg::call("template_match", alg::from_cv_mat(image), params);

        if (result["success"]) {
            alg::default_logger()->info("template match result: x={}, y={}, score={}",
                result["data"]["x"].get<int>(),
                result["data"]["y"].get<int>(),
                result["data"]["score"].get<double>());
        } else {
            alg::default_logger()->error("template match failed: {}", std::string(result["error"]));
            return 1;
        }
    } catch (const std::exception& e) {
        alg::default_logger()->error("exception: {}", e.what());
        return 1;
    }

    return 0;
}

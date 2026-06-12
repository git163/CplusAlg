#include "cplus_alg/alg_interface.h"
#include "cplus_alg/data_adapters/cv_mat_adapter.h"

#include <iostream>
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
            std::cout << "匹配结果: x=" << result["data"]["x"]
                      << ", y=" << result["data"]["y"]
                      << ", score=" << result["data"]["score"] << std::endl;
        } else {
            std::cerr << "调用失败: " << result["error"] << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

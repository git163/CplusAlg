#include <cplus_alg/alg_interface.h>

#ifdef CPLUS_ALG_HAS_OPENCV
#include <cplus_alg/data_adapters/cv_mat_adapter.h>
#include <opencv2/opencv.hpp>
#endif

#include <gtest/gtest.h>

TEST(TemplateMatch, DirectTransmit) {
#ifdef CPLUS_ALG_HAS_OPENCV
    cv::Mat image(100, 100, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Point(20, 30), cv::Point(29, 39), cv::Scalar(255), cv::FILLED);

    cv::Mat templ(10, 10, CV_8UC1, cv::Scalar(255));

    cplus_alg::call_params params;
    params.set("method", std::string("ccorr_normed"))
          .set_buffer("template", cplus_alg::from_cv_mat(templ));

    auto result = cplus_alg::call(
        cplus_alg::direct_transmit,
        "template_match",
        cplus_alg::from_cv_mat(image),
        params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_EQ(result["data"]["x"], 20);
    EXPECT_EQ(result["data"]["y"], 30);
    EXPECT_DOUBLE_EQ(result["data"]["score"], 1.0);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST(TemplateMatch, AutoTransmit) {
#ifdef CPLUS_ALG_HAS_OPENCV
    cv::Mat image(100, 100, CV_8UC1, cv::Scalar(0));
    cv::rectangle(image, cv::Point(20, 30), cv::Point(29, 39), cv::Scalar(255), cv::FILLED);

    cv::Mat templ(10, 10, CV_8UC1, cv::Scalar(255));

    cplus_alg::call_params params;
    params.set("method", std::string("ccorr_normed"))
          .set_buffer("template", cplus_alg::from_cv_mat(templ));

    auto result = cplus_alg::call(
        "template_match",
        cplus_alg::from_cv_mat(image),
        params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_EQ(result["data"]["x"], 20);
    EXPECT_EQ(result["data"]["y"], 30);
#else
    GTEST_SKIP() << "OpenCV not available";
#endif
}

TEST(TemplateMatch, MissingModule) {
    cplus_alg::call_params params;
    auto result = cplus_alg::call("nonexistent_module", cplus_alg::data_buffer{}, params);

    ASSERT_FALSE(result["success"]);
    EXPECT_NE(std::string(result["error"]).find("unknown module"), std::string::npos);
}

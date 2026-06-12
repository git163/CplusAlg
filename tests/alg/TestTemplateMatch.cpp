#include <cplus_alg/alg_interface.h>

#include <cstdint>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

namespace {

// 持有数据所有权并返回 data_buffer 视图的结构
struct owned_buffer {
    std::unique_ptr<std::vector<std::uint8_t>> data;
    cplus_alg::data_buffer buf;
};

// 构造 100x100 灰度图，在 (20, 30) 处放一个 10x10 的白色方块
owned_buffer make_test_image() {
    auto image = std::make_unique<std::vector<std::uint8_t>>(100 * 100, 0);
    for (int row = 30; row < 40; ++row) {
        for (int col = 20; col < 30; ++col) {
            (*image)[row * 100 + col] = 255;
        }
    }

    owned_buffer ob;
    ob.data = std::move(image);
    ob.buf.shape = {100, 100};
    ob.buf.dtype = "uint8";
    ob.buf.data = ob.data->data();
    ob.buf.size_bytes = ob.data->size();
    return ob;
}

// 构造 10x10 白色方块模板
owned_buffer make_test_template() {
    auto templ = std::make_unique<std::vector<std::uint8_t>>(10 * 10, 255);

    owned_buffer ob;
    ob.data = std::move(templ);
    ob.buf.shape = {10, 10};
    ob.buf.dtype = "uint8";
    ob.buf.data = ob.data->data();
    ob.buf.size_bytes = ob.data->size();
    return ob;
}

} // namespace

TEST(TemplateMatch, DirectTransmit) {
    auto image = make_test_image();
    auto templ = make_test_template();

    cplus_alg::call_params params;
    params.set("method", std::string("ccorr_normed"))
          .set_buffer("template", templ.buf);

    auto result = cplus_alg::call(
        cplus_alg::direct_transmit,
        "template_match",
        image.buf,
        params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_EQ(result["data"]["x"], 20);
    EXPECT_EQ(result["data"]["y"], 30);
    EXPECT_DOUBLE_EQ(result["data"]["score"], 1.0);
}

TEST(TemplateMatch, AutoTransmit) {
    auto image = make_test_image();
    auto templ = make_test_template();

    cplus_alg::call_params params;
    params.set("method", std::string("ccorr_normed"))
          .set_buffer("template", templ.buf);

    auto result = cplus_alg::call(
        "template_match",
        image.buf,
        params);

    ASSERT_TRUE(result["success"]) << result.value("error", "unknown error");
    EXPECT_EQ(result["data"]["x"], 20);
    EXPECT_EQ(result["data"]["y"], 30);
}

TEST(TemplateMatch, MissingModule) {
    cplus_alg::call_params params;
    auto result = cplus_alg::call("nonexistent_module", cplus_alg::data_buffer{}, params);

    ASSERT_FALSE(result["success"]);
    EXPECT_NE(std::string(result["error"]).find("unknown module"), std::string::npos);
}

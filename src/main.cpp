#include "cplus_alg/alg_interface.h"
#include "cplus_alg/logger.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace alg = cplus_alg;

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        // 构造合成图像：100x100 灰度图，在 (20, 30) 处放一个 10x10 的白色方块
        auto image = std::make_unique<std::vector<std::uint8_t>>(100 * 100, 0);
        for (int row = 30; row < 40; ++row) {
            for (int col = 20; col < 30; ++col) {
                (*image)[row * 100 + col] = 255;
            }
        }

        alg::data_buffer image_buf;
        image_buf.shape = {100, 100};
        image_buf.dtype = "uint8";
        image_buf.data = image->data();
        image_buf.size_bytes = image->size();

        // 模板：10x10 白色方块
        auto templ = std::make_unique<std::vector<std::uint8_t>>(10 * 10, 255);
        alg::data_buffer templ_buf;
        templ_buf.shape = {10, 10};
        templ_buf.dtype = "uint8";
        templ_buf.data = templ->data();
        templ_buf.size_bytes = templ->size();

        alg::call_params params;
        params.set("method", std::string("ccorr_normed"))
              .set_buffer("template", templ_buf);

        auto result = alg::call("template_match", image_buf, params);

        if (result["success"]) {
            CPLUS_ALG_LOG_INFO("template match result: x={}, y={}, score={}",
                result["data"]["x"].get<int>(),
                result["data"]["y"].get<int>(),
                result["data"]["score"].get<double>());
        } else {
            CPLUS_ALG_LOG_ERROR("template match failed: {}", std::string(result["error"]));
            return 1;
        }
    } catch (const std::exception& e) {
        CPLUS_ALG_LOG_ERROR("exception: {}", e.what());
        return 1;
    }

    return 0;
}

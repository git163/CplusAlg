#include "cplus_alg/alg_interface.h"
#include "cplus_alg/logger.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

namespace alg = cplus_alg;

static bool run_template_match_example() {
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
        return true;
    } else {
        CPLUS_ALG_LOG_ERROR("template match failed: {}", std::string(result["error"]));
        return false;
    }
}

static bool run_curve_fit_example() {
    // 生成带噪声的 erf 数据：y = erf(x) + noise
    constexpr int n = 100;
    constexpr double x_min = -3.0;
    constexpr double x_max = 3.0;

    std::vector<double> x(n);
    std::vector<double> y(n);
    std::mt19937 gen(42);
    std::normal_distribution<double> noise(0.0, 0.05);

    for (int i = 0; i < n; ++i) {
        double t = static_cast<double>(i) / (n - 1);
        x[i] = x_min + t * (x_max - x_min);
        y[i] = std::erf(x[i]) + noise(gen);
    }

    alg::call_params params;
    params.set("x", x)
          .set("y", y)
          .set("p0", std::vector<double>{1.0, 1.0, 0.0, 0.0})
          .set("plot_path", std::string("data/images/curve_fit.png"));

    auto result = alg::call("curve_fit", alg::data_buffer{}, params);

    if (result["success"]) {
        CPLUS_ALG_LOG_INFO("curve fit completed, plot saved to: {}",
            std::string(result["data"]["plot_path"]));
        CPLUS_ALG_LOG_INFO("fitted params: a={:.4f}, b={:.4f}, c={:.4f}, d={:.4f}",
            result["data"]["params"][0].get<double>(),
            result["data"]["params"][1].get<double>(),
            result["data"]["params"][2].get<double>(),
            result["data"]["params"][3].get<double>());
        return true;
    } else {
        CPLUS_ALG_LOG_ERROR("curve fit failed: {}", std::string(result["error"]));
        return false;
    }
}

int main(int /*argc*/, char* /*argv*/[]) {
    bool ok = true;
    try {
        ok &= run_template_match_example();
        ok &= run_curve_fit_example();
    } catch (const std::exception& e) {
        CPLUS_ALG_LOG_ERROR("exception: {}", e.what());
        return 1;
    }

    return ok ? 0 : 1;
}

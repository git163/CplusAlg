// src/main.cpp — pybind11 调用 matplotlib 绘制示例图

#include <iostream>
#include <stdexcept>

#include <pybind11/embed.h>
#include <pybind11/stl.h>

namespace py = pybind11;

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        // 启动 Python 解释器
        py::scoped_interpreter guard{};

        py::module_ sys = py::module_::import("sys");
        py::print("Python version:", sys.attr("version"));

        // 使用 Agg 后端，避免在无图形界面环境下报错
        py::module_ mpl = py::module_::import("matplotlib");
        mpl.attr("use")("Agg");

        // 导入 matplotlib.pyplot 并绘制简单曲线
        py::module_ plt = py::module_::import("matplotlib.pyplot");

        plt.attr("figure")(py::arg("figsize") = py::make_tuple(6, 4));
        plt.attr("plot")(py::make_tuple(0, 1, 2, 3, 4),
                         py::make_tuple(0, 1, 4, 9, 16),
                         py::arg("label") = "y = x^2",
                         py::arg("marker") = "o");
        plt.attr("title")("CplusAlg pybind11 demo");
        plt.attr("xlabel")("x");
        plt.attr("ylabel")("y");
        plt.attr("legend")();
        plt.attr("grid")(true);
        plt.attr("savefig")("demo_plot.png", py::arg("dpi") = 150);
        plt.attr("close")();

        std::cout << "demo_plot.png 已生成" << std::endl;
    } catch (const py::error_already_set& e) {
        std::cerr << "Python 调用失败: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

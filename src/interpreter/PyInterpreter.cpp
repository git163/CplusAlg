#include "PyInterpreter.h"

#include <pybind11/embed.h>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace py = pybind11;

PyInterpreter& PyInterpreter::Instance() {
    static PyInterpreter s_instance;
    return s_instance;
}

PyInterpreter::~PyInterpreter() {
    // 不在析构函数中调用 Finalize()，避免 static 对象销毁顺序问题
    // 进程退出时由操作系统回收资源
}

bool PyInterpreter::Initialize(const std::vector<std::string>& vecExtraPaths) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_bInitialized.load()) {
        // 即使已初始化，也应追加新传入的额外路径，避免幂等调用丢失配置
        SetupSysPath(vecExtraPaths);
        spdlog::warn("Python interpreter already initialized, applying extra paths only");
        return true;
    }

    try {
        py::initialize_interpreter();
        // 预热：在 GIL 下初始化 pybind11 内部状态，避免子线程并发初始化死锁
        {
            py::gil_scoped_acquire gil;
            (void)py::detail::get_internals();
        }
        SetupSysPath(vecExtraPaths);
        m_bInitialized.store(true);
        spdlog::info("Python interpreter initialized successfully");
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize Python interpreter: {}", e.what());
        return false;
    }
}

bool PyInterpreter::IsInitialized() const {
    return m_bInitialized.load();
}

void PyInterpreter::Finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_bInitialized.load()) {
        spdlog::warn("Python interpreter not initialized, nothing to finalize");
        return;
    }

    try {
        py::finalize_interpreter();
        m_bInitialized.store(false);
        spdlog::info("Python interpreter finalized");
    } catch (const std::exception& e) {
        spdlog::error("Error during Python interpreter finalization: {}", e.what());
    }
}

void PyInterpreter::SetupSysPath(const std::vector<std::string>& vecExtraPaths) {
    try {
        py::module_ sys = py::module_::import("sys");
        py::list pathList = sys.attr("path");

        // 基于可执行文件路径推导 python 目录，避免依赖当前工作目录
        std::filesystem::path default_path = "../python";
        try {
            py::list argv = sys.attr("argv").cast<py::list>();
            if (argv.size() > 0) {
                std::string argv0 = argv[0].cast<std::string>();
                std::filesystem::path exe_path(argv0);
                if (!exe_path.empty()) {
                    std::filesystem::path exe_dir = exe_path.parent_path();
                    std::filesystem::path project_dir = exe_dir.parent_path();
                    default_path = project_dir / "python";
                }
            }
        } catch (...) {
            // 忽略 sys.argv 读取失败，使用默认相对路径
        }
        pathList.append(default_path.string());
        spdlog::info("Added '{}' to sys.path", default_path.string());

        // 添加用户指定的额外路径
        for (const auto& strPath : vecExtraPaths) {
            if (!strPath.empty()) {
                pathList.append(strPath);
                spdlog::info("Added '{}' to sys.path", strPath);
            }
        }
    } catch (py::error_already_set& e) {
        spdlog::error("Failed to setup sys.path: {}", e.what());
        PyErr_Print();
    }
}

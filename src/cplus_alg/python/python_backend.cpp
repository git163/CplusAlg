#include "cplus_alg/python/python_backend.h"

#include "cplus_alg/alg_interface.h"
#include "cplus_alg/logger.h"
#include "cplus_alg/python/type_converter.h"
#include "interpreter/PyInterpreter.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <filesystem>
#include <memory>
#include <stdexcept>

namespace py = pybind11;

namespace cplus_alg {
namespace python {

namespace {

// Python 解释器单例，程序生命周期内保持。
// 使用堆分配并故意不 delete，避免静态析构阶段 finalize Python 解释器时
// 与 pybind11 / numpy 的清理逻辑发生竞态或顺序问题，从而引发偶现 SIGTRAP。
class python_runtime {
public:
    static python_runtime& instance() {
        static python_runtime* inst = new python_runtime();
        return *inst;
    }

    py::object registry() {
        ensure_initialized();
        return registry_;
    }

    // 显式关闭解释器；外部可在需要干净退出的场景主动调用。
    void shutdown() {
        if (!initialized_) {
            return;
        }
        // 退出阶段不要再写日志，spdlog 的 sink 等静态对象可能已处于析构过程中。
        registry_ = py::object();
        initialized_ = false;
    }

private:
    python_runtime() = default;

    void ensure_initialized() {
        if (initialized_) return;

        CPLUS_ALG_LOG_DEBUG("initializing embedded python interpreter");
        auto& interp = PyInterpreter::Instance();
        if (!interp.Initialize()) {
            throw std::runtime_error("failed to initialize embedded python interpreter via PyInterpreter");
        }
        try {
            add_alg_path();
            registry_ = py::module_::import("alg.core.registry");
            initialized_ = true;
            CPLUS_ALG_LOG_DEBUG("python interpreter initialized, alg.core.registry loaded");
        } catch (...) {
            CPLUS_ALG_LOG_ERROR("failed to initialize python runtime");
            registry_ = py::object();
            throw;
        }
    }

    void add_alg_path() {
        py::module_ sys = py::module_::import("sys");

        std::filesystem::path exe_dir;
        try {
            // 优先从 argv[0] 推导
            py::list argv = sys.attr("argv").cast<py::list>();
            if (argv.size() > 0) {
                py::str argv0 = argv[0];
                std::string argv0_str = argv0.cast<std::string>();
                exe_dir = std::filesystem::path(argv0_str).parent_path();
            }
        } catch (...) {
            // 忽略
        }

        if (exe_dir.empty()) {
            exe_dir = std::filesystem::current_path();
        }

        // 寻找包含 alg/__init__.py 的目录，把该目录的父目录加入 sys.path。
        // Python 导入 alg 包时需要的是 alg 的父目录，而不是 alg 本身。
        std::vector<std::filesystem::path> candidates = {
#ifdef CPLUS_ALG_SOURCE_DIR
            std::filesystem::path(CPLUS_ALG_SOURCE_DIR),
#endif
            exe_dir / ".." / "..",
            exe_dir / "..",
            exe_dir,
            std::filesystem::current_path(),
        };

        for (const auto& p : candidates) {
            if (std::filesystem::exists(p / "alg" / "__init__.py")) {
                CPLUS_ALG_LOG_DEBUG("adding alg parent directory to sys.path: {}", p.string());
                sys.attr("path").attr("append")(p.string());
                return;
            }
        }

        // 兜底：把当前工作目录加入，alg 包应在当前目录下
        CPLUS_ALG_LOG_WARN("alg package not found in candidates, falling back to current directory");
        sys.attr("path").attr("append")(std::filesystem::current_path().string());
    }

    bool initialized_ = false;
    py::object registry_;
};

} // namespace

backend::dispatch_result python_backend::dispatch(
    const std::string& module_name,
    const std::optional<std::variant<data_buffer, shm_handle>>& input,
    const nlohmann::json& json_params,
    const std::unordered_map<std::string, data_buffer>& buffer_params) {
    try {
        auto& rt = python_runtime::instance();
        py::object registry = rt.registry();

        CPLUS_ALG_LOG_DEBUG("dispatching call to module: {}", module_name);

        py::object py_input;
        if (input.has_value()) {
            data_or_handle doh = std::visit(
                [](const auto& arg) -> data_or_handle { return arg; },
                *input);
            py_input = input_to_py(doh);
        } else {
            py_input = py::none();
        }

        py::dict py_params = params_to_py(json_params, buffer_params);

        py::object result = registry.attr("dispatch")(
            module_name, py_input, py_params);

        CPLUS_ALG_LOG_DEBUG("module {} returned successfully", module_name);

        nlohmann::json j = py_to_json(result);
        if (j.value("success", false)) {
            return backend::dispatch_result::ok(j.value("data", nlohmann::json::object()));
        }
        return backend::dispatch_result::fail(
            j.value("error", "unknown error"),
            j.value("error_type", "UnknownError"));
    } catch (const py::error_already_set& e) {
        CPLUS_ALG_LOG_ERROR("python exception in module {}: {}", module_name, e.what());
        return backend::dispatch_result::fail(e.what(), "PythonException");
    } catch (const std::exception& e) {
        CPLUS_ALG_LOG_ERROR("c++ exception in module {}: {}", module_name, e.what());
        return backend::dispatch_result::fail(e.what(), "CxxException");
    }
}

bool python_backend::available() const {
    try {
        auto& interp = PyInterpreter::Instance();
        if (!interp.IsInitialized()) {
            return false;
        }
        auto& rt = python_runtime::instance();
        rt.registry();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace python
} // namespace cplus_alg

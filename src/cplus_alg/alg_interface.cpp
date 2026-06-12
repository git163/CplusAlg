#include "cplus_alg/alg_interface.h"
#include "cplus_alg/logger.h"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>

namespace py = pybind11;

namespace cplus_alg {

namespace {

constexpr std::size_t k_small_data_threshold_bytes = 1 * 1024 * 1024;

// Python 解释器单例，程序生命周期内保持。
class python_runtime {
public:
    static python_runtime& instance() {
        static python_runtime inst;
        return inst;
    }

    py::object registry() {
        ensure_initialized();
        return registry_;
    }

private:
    python_runtime() = default;

    void ensure_initialized() {
        if (initialized_) return;

        CPLUS_ALG_LOG_DEBUG("initializing embedded python interpreter");
        guard_ = std::make_unique<py::scoped_interpreter>();
        try {
            add_alg_path();
            registry_ = py::module_::import("alg.core.registry");
            initialized_ = true;
            CPLUS_ALG_LOG_DEBUG("python interpreter initialized, alg.core.registry loaded");
        } catch (...) {
            // 导入失败时释放解释器，避免后续调用触发 "interpreter already running"
            CPLUS_ALG_LOG_ERROR("failed to initialize python runtime, releasing interpreter");
            guard_.reset();
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
    std::unique_ptr<py::scoped_interpreter> guard_;
    py::object registry_;
};

std::string dtype_to_str(int cv_depth) {
    switch (cv_depth) {
        case 0: return "uint8";
        case 1: return "int8";
        case 2: return "uint16";
        case 3: return "int16";
        case 4: return "int32";
        case 5: return "float32";
        case 6: return "float64";
        default: return "uint8";
    }
}

py::object buffer_to_numpy(const data_buffer& buf) {
    // 空数据直接映射为 None，避免 numpy 对空 dtype/shape 抛异常，
    // 同时让上层 Python 代码自行判断输入是否合法。
    if (buf.data == nullptr || buf.size_bytes == 0) {
        return py::none();
    }

    py::module_ np = py::module_::import("numpy");

    py::object dtype = np.attr("dtype")(buf.dtype);

    py::list shape;
    for (int s : buf.shape) {
        shape.append(s);
    }

    // 用 memoryview 做零拷贝视图
    py::object mv = py::memoryview::from_memory(
        buf.data,
        buf.size_bytes,
        false); // 只读视图，调用方保证生命周期

    py::object array = np.attr("frombuffer")(mv, py::arg("dtype") = dtype);
    array.attr("shape") = shape;
    return array;
}

py::dict buffer_to_py_input(const data_buffer& buf) {
    py::dict result;
    result["type"] = "buffer";
    result["array"] = buffer_to_numpy(buf);
    result["shape"] = buf.shape;
    result["dtype"] = buf.dtype;
    result["size_bytes"] = buf.size_bytes;
    return result;
}

py::dict shm_handle_to_py_input(const shm_handle& handle) {
    py::dict result;
    result["type"] = "shm";

    py::dict h;
    h["name"] = handle.name;
    h["shape"] = handle.shape;
    h["dtype"] = handle.dtype;
    h["size_bytes"] = handle.size_bytes;

    result["handle"] = h;
    return result;
}

py::object input_to_py(const data_or_handle& input) {
    return std::visit(
        [](const auto& arg) -> py::object {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, data_buffer>) {
                return buffer_to_py_input(arg);
            } else {
                return shm_handle_to_py_input(arg);
            }
        },
        input);
}

py::object json_to_py(const nlohmann::json& j) {
    if (j.is_null()) {
        return py::none();
    }
    if (j.is_boolean()) {
        return py::bool_(j.get<bool>());
    }
    if (j.is_number_integer()) {
        return py::int_(j.get<long long>());
    }
    if (j.is_number_unsigned()) {
        return py::int_(static_cast<long long>(j.get<std::uint64_t>()));
    }
    if (j.is_number_float()) {
        return py::float_(j.get<double>());
    }
    if (j.is_string()) {
        return py::str(j.get<std::string>());
    }
    if (j.is_array()) {
        py::list lst;
        for (const auto& item : j) {
            lst.append(json_to_py(item));
        }
        return lst;
    }
    if (j.is_object()) {
        py::dict d;
        for (const auto& [key, value] : j.items()) {
            d[key.c_str()] = json_to_py(value);
        }
        return d;
    }
    return py::str(j.dump());
}

py::dict params_to_py(const call_params& params) {
    py::dict result = json_to_py(params.json());

    for (const auto& [key, buf] : params.buffers()) {
        result[key.c_str()] = buffer_to_numpy(buf);
    }

    return result;
}

nlohmann::json py_to_json(const py::handle& obj) {
    if (py::isinstance<py::dict>(obj)) {
        nlohmann::json j;
        for (const auto& item : py::cast<py::dict>(obj)) {
            std::string key = py::cast<std::string>(item.first);
            j[key] = py_to_json(item.second);
        }
        return j;
    }
    if (py::isinstance<py::list>(obj) || py::isinstance<py::tuple>(obj)) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& item : obj) {
            j.push_back(py_to_json(item));
        }
        return j;
    }
    if (py::isinstance<py::bool_>(obj)) {
        return py::cast<bool>(obj);
    }
    if (py::isinstance<py::int_>(obj)) {
        return py::cast<long long>(obj);
    }
    if (py::isinstance<py::float_>(obj)) {
        return py::cast<double>(obj);
    }
    if (py::isinstance<py::str>(obj)) {
        return py::cast<std::string>(obj);
    }
    if (obj.is_none()) {
        return nullptr;
    }

    // 其他类型转成字符串
    return py::cast<std::string>(py::str(obj));
}

std::string generate_shm_name() {
    static thread_local std::mt19937 gen(
        static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<int> dist(0, 15);

    std::ostringstream oss;
    oss << "cplusalg_";
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << dist(gen);
    }
    return oss.str();
}

nlohmann::json do_call(
    const std::string& module_name,
    const std::optional<data_or_handle>& input,
    const call_params& params) {
    try {
        auto& rt = python_runtime::instance();
        py::object registry = rt.registry();  // 先初始化解释器

        CPLUS_ALG_LOG_DEBUG("dispatching call to module: {}", module_name);

        py::object py_input;
        if (input.has_value()) {
            py_input = std::visit(
                [](const auto& arg) -> py::object {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, data_buffer>) {
                        return buffer_to_py_input(arg);
                    } else {
                        return shm_handle_to_py_input(arg);
                    }
                },
                *input);
        } else {
            py_input = py::none();
        }

        py::dict py_params = params_to_py(params);

        py::object result = registry.attr("dispatch")(
            module_name, py_input, py_params);

        CPLUS_ALG_LOG_DEBUG("module {} returned successfully", module_name);
        return py_to_json(result);
    } catch (const py::error_already_set& e) {
        CPLUS_ALG_LOG_ERROR("python exception in module {}: {}", module_name, e.what());
        nlohmann::json err;
        err["success"] = false;
        err["error"] = e.what();
        err["error_type"] = "PythonException";
        return err;
    } catch (const std::exception& e) {
        CPLUS_ALG_LOG_ERROR("c++ exception in module {}: {}", module_name, e.what());
        nlohmann::json err;
        err["success"] = false;
        err["error"] = e.what();
        err["error_type"] = "CxxException";
        return err;
    }
}

} // namespace

nlohmann::json call(
    const std::string& module_name,
    const data_or_handle& input,
    const call_params& params) {
    return std::visit(
        [&](const auto& arg) -> nlohmann::json {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, data_buffer>) {
                if (arg.size_bytes <= k_small_data_threshold_bytes) {
                    return call(direct_transmit, module_name, arg, params);
                }
                return call(shm_transmit, module_name, arg, params);
            } else {
                return do_call(module_name, std::optional<data_or_handle>{input}, params);
            }
        },
        input);
}

nlohmann::json call(
    direct_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const call_params& params) {
    return do_call(module_name, std::optional<data_or_handle>{input}, params);
}

nlohmann::json call(
    shm_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const call_params& params) {
    std::string name = generate_shm_name();
    shm_buffer buffer(name, input.size_bytes);
    std::memcpy(buffer.data(), input.data, input.size_bytes);

    shm_handle handle;
    handle.name = buffer.name();
    handle.shape = input.shape;
    handle.dtype = input.dtype;
    handle.size_bytes = input.size_bytes;

    auto result = do_call(module_name, std::optional<data_or_handle>{handle}, params);
    // buffer 在此处析构，自动 munmap + shm_unlink
    return result;
}

nlohmann::json call(
    const std::string& module_name,
    const call_params& params) {
    return do_call(module_name, std::optional<data_or_handle>{}, params);
}

} // namespace cplus_alg

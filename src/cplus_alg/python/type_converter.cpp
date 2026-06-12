#include "cplus_alg/python/type_converter.h"

#include <pybind11/stl.h>

#include <string>

namespace py = pybind11;

namespace cplus_alg {
namespace python {

namespace {

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

} // namespace

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

py::dict params_to_py(const call_params& params) {
    return params_to_py(params.json(), params.buffers());
}

py::dict params_to_py(
    const nlohmann::json& json_params,
    const std::unordered_map<std::string, data_buffer>& buffer_params) {
    py::dict result = json_to_py(json_params);

    for (const auto& [key, buf] : buffer_params) {
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

} // namespace python
} // namespace cplus_alg

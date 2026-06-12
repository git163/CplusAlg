#ifndef CPLUS_ALG_PYTHON_TYPE_CONVERTER_H_
#define CPLUS_ALG_PYTHON_TYPE_CONVERTER_H_

#include "cplus_alg/alg_interface.h"

#include <nlohmann/json.hpp>
#include <pybind11/pybind11.h>

namespace cplus_alg {
namespace python {

// 将 data_buffer 或 shm_handle 转换为 Python 可识别的输入字典
pybind11::object input_to_py(const data_or_handle& input);

// 将 call_params 转换为 Python 字典（含 json 字段与 buffer 字段）
pybind11::dict params_to_py(const call_params& params);

// 将 nlohmann::json 与 buffer 参数字典转换为 Python 字典
pybind11::dict params_to_py(
    const nlohmann::json& json_params,
    const std::unordered_map<std::string, data_buffer>& buffer_params);

// 将 Python 对象转换回 nlohmann::json
nlohmann::json py_to_json(const pybind11::handle& obj);

} // namespace python
} // namespace cplus_alg

#endif // CPLUS_ALG_PYTHON_TYPE_CONVERTER_H_

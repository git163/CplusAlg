#ifndef CPLUS_ALG_BACKEND_BACKEND_INTERFACE_H_
#define CPLUS_ALG_BACKEND_BACKEND_INTERFACE_H_

#include "cplus_alg/backend/dispatch_result.h"
#include "cplus_alg/data_buffer.h"
#include "cplus_alg/shm_handle.h"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>

namespace cplus_alg {
namespace backend {

// 算法后端抽象接口，支持 Python 后端、纯 C++ 后端、远程服务后端等实现
class backend_interface {
public:
    virtual ~backend_interface() = default;

    virtual dispatch_result dispatch(
        const std::string& module_name,
        const std::optional<std::variant<data_buffer, shm_handle>>& input,
        const nlohmann::json& json_params,
        const std::unordered_map<std::string, data_buffer>& buffer_params) = 0;

    virtual bool available() const = 0;
};

} // namespace backend
} // namespace cplus_alg

#endif // CPLUS_ALG_BACKEND_BACKEND_INTERFACE_H_

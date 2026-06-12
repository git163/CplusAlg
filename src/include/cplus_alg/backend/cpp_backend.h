#ifndef CPLUS_ALG_BACKEND_CPP_BACKEND_H_
#define CPLUS_ALG_BACKEND_CPP_BACKEND_H_

#include "cplus_alg/backend/backend_interface.h"

namespace cplus_alg {
namespace backend {

// 简单 C++ 后端示例，验证 backend_interface 抽象可用
class cpp_backend : public backend_interface {
public:
    cpp_backend() = default;
    ~cpp_backend() override = default;

    dispatch_result dispatch(
        const std::string& module_name,
        const std::optional<std::variant<data_buffer, shm_handle>>& input,
        const nlohmann::json& json_params,
        const std::unordered_map<std::string, data_buffer>& buffer_params) override;

    bool available() const override { return true; }
};

} // namespace backend
} // namespace cplus_alg

#endif // CPLUS_ALG_BACKEND_CPP_BACKEND_H_

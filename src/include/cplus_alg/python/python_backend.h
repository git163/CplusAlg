#ifndef CPLUS_ALG_PYTHON_PYTHON_BACKEND_H_
#define CPLUS_ALG_PYTHON_PYTHON_BACKEND_H_

#include "cplus_alg/backend/backend_interface.h"

namespace cplus_alg {
namespace python {

// Python 后端实现：内嵌 Python 解释器，调用 alg.core.registry.dispatch
class python_backend : public backend::backend_interface {
public:
    python_backend() = default;
    ~python_backend() override = default;

    backend::dispatch_result dispatch(
        const std::string& module_name,
        const std::optional<std::variant<data_buffer, shm_handle>>& input,
        const nlohmann::json& json_params,
        const std::unordered_map<std::string, data_buffer>& buffer_params) override;

    bool available() const override;
};

} // namespace python
} // namespace cplus_alg

#endif // CPLUS_ALG_PYTHON_PYTHON_BACKEND_H_

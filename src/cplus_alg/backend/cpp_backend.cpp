#include "cplus_alg/backend/cpp_backend.h"

#include "cplus_alg/logger.h"

namespace cplus_alg {
namespace backend {

dispatch_result cpp_backend::dispatch(
    const std::string& module_name,
    const std::optional<std::variant<data_buffer, shm_handle>>& input,
    const nlohmann::json& json_params,
    const std::unordered_map<std::string, data_buffer>& /*buffer_params*/) {
    if (module_name != "cpp_echo") {
        return dispatch_result::fail(
            "unknown module: " + module_name,
            "ModuleNotFoundError");
    }

    CPLUS_ALG_LOG_DEBUG("cpp_backend dispatching module: {}", module_name);

    nlohmann::json data;
    data["has_input"] = input.has_value();
    data["module"] = module_name;
    if (json_params.contains("value")) {
        data["value"] = json_params["value"];
    } else {
        data["value"] = nullptr;
    }
    return dispatch_result::ok(data);
}

} // namespace backend
} // namespace cplus_alg

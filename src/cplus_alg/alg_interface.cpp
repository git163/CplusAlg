#include "cplus_alg/alg_interface.h"
#include "cplus_alg/backend/backend_interface.h"
#include "cplus_alg/logger.h"
#include "cplus_alg/python/python_backend.h"
#include "cplus_alg/transport/direct_transport.h"
#include "cplus_alg/transport/shm_transport.h"

#include <memory>
#include <stdexcept>

namespace cplus_alg {

namespace {

constexpr std::size_t k_small_data_threshold_bytes = 1 * 1024 * 1024;

// 默认后端：优先使用 Python 后端。后续可通过 setter 扩展为可配置。
backend::backend_interface& default_backend() {
    static python::python_backend backend;
    return backend;
}

nlohmann::json do_dispatch(
    const std::string& module_name,
    const std::optional<data_or_handle>& input,
    const call_params& params) {
    auto result = default_backend().dispatch(
        module_name,
        input,
        params.json(),
        params.buffers());
    return result.to_json();
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
                std::unique_ptr<transport::transport_strategy> strategy;
                if (arg.size_bytes <= k_small_data_threshold_bytes) {
                    strategy = std::make_unique<transport::direct_transport>();
                } else {
                    strategy = std::make_unique<transport::shm_transport>();
                }
                auto prepared = strategy->prepare_input(arg);
                return do_dispatch(module_name, std::optional<data_or_handle>{prepared}, params);
            } else {
                return do_dispatch(module_name, std::optional<data_or_handle>{input}, params);
            }
        },
        input);
}

nlohmann::json call(
    direct_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const call_params& params) {
    transport::direct_transport strategy;
    auto prepared = strategy.prepare_input(input);
    return do_dispatch(module_name, std::optional<data_or_handle>{prepared}, params);
}

nlohmann::json call(
    shm_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const call_params& params) {
    transport::shm_transport strategy;
    auto prepared = strategy.prepare_input(input);
    return do_dispatch(module_name, std::optional<data_or_handle>{prepared}, params);
}

nlohmann::json call(
    const std::string& module_name,
    const call_params& params) {
    return do_dispatch(module_name, std::optional<data_or_handle>{}, params);
}

} // namespace cplus_alg

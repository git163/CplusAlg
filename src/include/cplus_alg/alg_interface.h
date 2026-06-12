#ifndef CPLUS_ALG_ALG_INTERFACE_H
#define CPLUS_ALG_ALG_INTERFACE_H

#include "cplus_alg/data_buffer.h"
#include "cplus_alg/shm_handle.h"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <variant>

namespace cplus_alg {

using data_or_handle = std::variant<data_buffer, shm_handle>;

// 传输模式 tag
struct auto_transmit_t {};
struct direct_transmit_t {};
struct shm_transmit_t {};

inline constexpr auto_transmit_t auto_transmit{};
inline constexpr direct_transmit_t direct_transmit{};
inline constexpr shm_transmit_t shm_transmit{};

// 调用参数容器，支持标量 JSON 值和 data_buffer 缓冲区。
class call_params {
public:
    call_params() = default;

    template <typename T>
    call_params& set(const std::string& key, T&& value) {
        json_[key] = std::forward<T>(value);
        return *this;
    }

    call_params& set_buffer(const std::string& key, const data_buffer& buf) {
        buffers_[key] = buf;
        return *this;
    }

    const nlohmann::json& json() const { return json_; }
    const std::unordered_map<std::string, data_buffer>& buffers() const { return buffers_; }

private:
    nlohmann::json json_ = nlohmann::json::object();
    std::unordered_map<std::string, data_buffer> buffers_;
};

// 自动按大小选择传输方式
nlohmann::json call(
    const std::string& module_name,
    const data_or_handle& input,
    const call_params& params = {});

// 强制直接传内存（编译期只接受 data_buffer）
nlohmann::json call(
    direct_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const call_params& params = {});

// 强制走共享内存（编译期只接受 data_buffer，由 C++ 创建 shm）
nlohmann::json call(
    shm_transmit_t,
    const std::string& module_name,
    const data_buffer& input,
    const call_params& params = {});

// 无数据输入，纯参数调用
nlohmann::json call(
    const std::string& module_name,
    const call_params& params = {});

} // namespace cplus_alg

#endif // CPLUS_ALG_ALG_INTERFACE_H

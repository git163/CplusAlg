#include "cplus_alg/transport/shm_transport.h"

#include <chrono>
#include <random>
#include <sstream>

namespace cplus_alg {
namespace transport {

namespace {

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

} // namespace

data_or_handle shm_transport::prepare_input(const data_buffer& buf) {
    std::string name = generate_shm_name();
    buffer_ = std::make_unique<shm_buffer>(name, buf.size_bytes);
    std::memcpy(buffer_->data(), buf.data, buf.size_bytes);

    shm_handle handle;
    handle.name = buffer_->name();
    handle.shape = buf.shape;
    handle.dtype = buf.dtype;
    handle.size_bytes = buf.size_bytes;
    return handle;
}

} // namespace transport
} // namespace cplus_alg

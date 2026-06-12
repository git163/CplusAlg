#ifndef CPLUS_ALG_TRANSPORT_SHM_TRANSPORT_H_
#define CPLUS_ALG_TRANSPORT_SHM_TRANSPORT_H_

#include "cplus_alg/transport/transport_strategy.h"
#include "cplus_alg/shm_handle.h"

#include <memory>

namespace cplus_alg {
namespace transport {

// 共享内存传输：将数据写入 shm_buffer，返回 shm_handle
// 策略对象持有 shm_buffer 生命周期，直到调用结束析构
class shm_transport : public transport_strategy {
public:
    data_or_handle prepare_input(const data_buffer& buf) override;

private:
    std::unique_ptr<shm_buffer> buffer_;
};

} // namespace transport
} // namespace cplus_alg

#endif // CPLUS_ALG_TRANSPORT_SHM_TRANSPORT_H_

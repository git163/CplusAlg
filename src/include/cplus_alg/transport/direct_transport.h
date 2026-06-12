#ifndef CPLUS_ALG_TRANSPORT_DIRECT_TRANSPORT_H_
#define CPLUS_ALG_TRANSPORT_DIRECT_TRANSPORT_H_

#include "cplus_alg/transport/transport_strategy.h"

namespace cplus_alg {
namespace transport {

// 直接传输：返回 data_buffer 的零拷贝视图
class direct_transport : public transport_strategy {
public:
    data_or_handle prepare_input(const data_buffer& buf) override;
};

} // namespace transport
} // namespace cplus_alg

#endif // CPLUS_ALG_TRANSPORT_DIRECT_TRANSPORT_H_

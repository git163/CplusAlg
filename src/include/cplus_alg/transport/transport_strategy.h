#ifndef CPLUS_ALG_TRANSPORT_TRANSPORT_STRATEGY_H_
#define CPLUS_ALG_TRANSPORT_TRANSPORT_STRATEGY_H_

#include "cplus_alg/alg_interface.h"

namespace cplus_alg {
namespace transport {

// 输入数据传输策略接口：将 data_buffer 转换为可直接传入后端的 data_or_handle
class transport_strategy {
public:
    virtual ~transport_strategy() = default;
    virtual data_or_handle prepare_input(const data_buffer& buf) = 0;
};

} // namespace transport
} // namespace cplus_alg

#endif // CPLUS_ALG_TRANSPORT_TRANSPORT_STRATEGY_H_

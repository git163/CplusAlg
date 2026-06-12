#include "cplus_alg/transport/direct_transport.h"

namespace cplus_alg {
namespace transport {

data_or_handle direct_transport::prepare_input(const data_buffer& buf) {
    return buf;
}

} // namespace transport
} // namespace cplus_alg

#ifndef CPLUS_ALG_DATA_BUFFER_H
#define CPLUS_ALG_DATA_BUFFER_H

#include <cstddef>
#include <string>
#include <string>
#include <vector>

namespace cplus_alg {

// 通用数据缓冲区描述，作为 C++ 与 Python 之间的通用数据视图。
// 本身不拥有内存生命周期，调用方需保证 data 指针在传输期间有效。
struct data_buffer {
    std::vector<int> shape;      // 任意维度
    std::string dtype;           // "uint8", "float32", "int32", ...
    void* data = nullptr;        // 连续内存指针
    std::size_t size_bytes = 0;  // 总字节数
};

} // namespace cplus_alg

#endif // CPLUS_ALG_DATA_BUFFER_H

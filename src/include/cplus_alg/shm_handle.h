#ifndef CPLUS_ALG_SHM_HANDLE_H
#define CPLUS_ALG_SHM_HANDLE_H

#include "cplus_alg/data_buffer.h"

#include <cstddef>
#include <string>
#include <vector>

namespace cplus_alg {

// 共享内存句柄，用于大数据传输时只传递句柄而非复制数据。
struct shm_handle {
    std::string name;            // POSIX 共享内存对象名
    std::vector<int> shape;
    std::string dtype;
    std::size_t size_bytes = 0;
};

// RAII 管理 POSIX 共享内存。
// 构造时创建 shm 并映射，析构时自动 munmap 并 shm_unlink。
class shm_buffer {
public:
    shm_buffer(const std::string& name, std::size_t size);
    ~shm_buffer() noexcept;

    void* data() const noexcept { return addr_; }
    std::size_t size() const noexcept { return size_; }
    const std::string& name() const noexcept { return name_; }

    shm_buffer(const shm_buffer&) = delete;
    shm_buffer& operator=(const shm_buffer&) = delete;
    shm_buffer(shm_buffer&& other) noexcept;
    shm_buffer& operator=(shm_buffer&& other) noexcept;

private:
    void release() noexcept;

    std::string name_;
    void* addr_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace cplus_alg

#endif // CPLUS_ALG_SHM_HANDLE_H

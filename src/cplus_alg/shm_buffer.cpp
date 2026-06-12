#include "cplus_alg/shm_handle.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdexcept>
#include <string>

namespace cplus_alg {

shm_buffer::shm_buffer(const std::string& name, std::size_t size)
    : name_(name), size_(size) {
    int fd = shm_open(name_.c_str(), O_CREAT | O_RDWR, 0666);
    if (fd < 0) {
        throw std::runtime_error(std::string("shm_open failed: ") + std::strerror(errno));
    }

    if (ftruncate(fd, static_cast<off_t>(size_)) < 0) {
        ::close(fd);
        shm_unlink(name_.c_str());
        throw std::runtime_error(std::string("ftruncate failed: ") + std::strerror(errno));
    }

    addr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);

    if (addr_ == MAP_FAILED) {
        addr_ = nullptr;
        shm_unlink(name_.c_str());
        throw std::runtime_error(std::string("mmap failed: ") + std::strerror(errno));
    }
}

shm_buffer::~shm_buffer() noexcept {
    release();
}

shm_buffer::shm_buffer(shm_buffer&& other) noexcept
    : name_(std::move(other.name_)), addr_(other.addr_), size_(other.size_) {
    other.addr_ = nullptr;
    other.size_ = 0;
}

shm_buffer& shm_buffer::operator=(shm_buffer&& other) noexcept {
    if (this != &other) {
        release();
        name_ = std::move(other.name_);
        addr_ = other.addr_;
        size_ = other.size_;
        other.addr_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

void shm_buffer::release() noexcept {
    if (addr_ != nullptr && addr_ != MAP_FAILED) {
        munmap(addr_, size_);
        addr_ = nullptr;
    }
    if (!name_.empty()) {
        shm_unlink(name_.c_str());
        name_.clear();
    }
    size_ = 0;
}

} // namespace cplus_alg

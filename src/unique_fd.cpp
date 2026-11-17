#include "miniposix/unique_fd.hpp"

#include <utility>
#include <unistd.h>

namespace miniposix {

UniqueFd::UniqueFd(int fd) noexcept : fd_(fd) {}

UniqueFd::~UniqueFd() noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept
    : fd_(std::exchange(other.fd_, -1)) {}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
        reset(other.release());
    }
    return *this;
}

int UniqueFd::get() const noexcept {
    return fd_;
}

int UniqueFd::release() noexcept {
    return std::exchange(fd_, -1);
}

void UniqueFd::reset(int fd) noexcept {
    if (fd_ == fd) {
        return;
    }
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = fd;
}

} // namespace miniposix

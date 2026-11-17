#include "miniposix/unique_fd.hpp"

#include <unistd.h>

namespace miniposix {

UniqueFd::UniqueFd(int fd) noexcept : fd_(fd) {}

UniqueFd::~UniqueFd() noexcept {
    reset();
}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    if(this != &other) {
        reset(other.fd_);
        other.fd_ = -1;
    }
    return *this;
}

int UniqueFd::get() const noexcept {
    return fd_;
}

int UniqueFd::release() noexcept {
    const int fd = fd_;
    fd_ = -1;
    return fd;
}

void UniqueFd::reset(int fd) noexcept {
    if(fd_ == fd) return;
    if(fd_ != -1) ::close(fd_);
    fd_ = fd;
}

} // namespace miniposix

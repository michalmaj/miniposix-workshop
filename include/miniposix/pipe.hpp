#pragma once

#include "miniposix/unique_fd.hpp"

#include <system_error>

namespace miniposix {

// Owns both ends of one anonymous POSIX pipe. Read from read_end(), write to
// write_end(). Moving transfers ownership; copying is disabled.
class Pipe {
public:
    // Creates a pipe, or throws std::system_error with the POSIX error code.
    // On failure no descriptor may leak.
    [[nodiscard]] static Pipe create();

    Pipe(const Pipe&) = delete;
    Pipe& operator=(const Pipe&) = delete;
    Pipe(Pipe&&) noexcept = default;
    Pipe& operator=(Pipe&&) noexcept = default;

    [[nodiscard]] UniqueFd& read_end() noexcept { return read_end_; }
    [[nodiscard]] const UniqueFd& read_end() const noexcept { return read_end_; }
    [[nodiscard]] UniqueFd& write_end() noexcept { return write_end_; }
    [[nodiscard]] const UniqueFd& write_end() const noexcept { return write_end_; }

private:
    Pipe() = default;

    UniqueFd read_end_;
    UniqueFd write_end_;
};

} // namespace miniposix

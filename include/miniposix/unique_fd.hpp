#pragma once

namespace miniposix {

// Owns one POSIX file descriptor. -1 means empty; every other value is owned.
// Do not close an owned descriptor through a raw POSIX call.
class UniqueFd {
public:
    UniqueFd() noexcept = default;

    // Adopts fd. The caller must not close it after this call.
    explicit UniqueFd(int fd) noexcept;
    ~UniqueFd() noexcept;

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;

    [[nodiscard]] int get() const noexcept;

    // Gives ownership back to the caller and leaves this object empty.
    [[nodiscard]] int release() noexcept;

    // Closes the current descriptor, then adopts fd. reset(get()) is a no-op.
    // reset() closes and leaves this object empty.
    void reset(int fd = -1) noexcept;

private:
    int fd_ = -1;
};

} // namespace miniposix

#include "miniposix/pipe.hpp"

#include <cerrno>
#include <system_error>
#include <unistd.h>

namespace miniposix {

Pipe Pipe::create() {
    // TODO: Use the portable POSIX pipe() call; put its read and write descriptors
    // into the two UniqueFd members. If creation fails, throw
    // std::system_error with errno and do not leak either descriptor.
    return Pipe{};
}

} // namespace miniposix

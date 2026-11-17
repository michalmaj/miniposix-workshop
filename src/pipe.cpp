#include "miniposix/pipe.hpp"

#include <cerrno>
#include <system_error>
#include <unistd.h>

namespace miniposix {

Pipe Pipe::create() {
    int raw[2];
    if (::pipe(raw) != 0) {
        const int error = errno;
        throw std::system_error(error, std::generic_category(), "pipe");
    }

    Pipe result;
    result.read_end_ = UniqueFd{raw[0]};
    result.write_end_ = UniqueFd{raw[1]};
    return result;
}

} // namespace miniposix

#include "miniposix/pipe.hpp"

#include <cerrno>
#include <system_error>
#include <unistd.h>

namespace miniposix {

Pipe Pipe::create() {
    int ends[2] = {-1, -1};
    if(::pipe(ends) == -1) {
        const int error = errno;
        throw std::system_error(error, std::generic_category(), "pipe");
    }

    Pipe result;
    result.read_end_.reset(ends[0]);
    result.write_end_.reset(ends[1]);
    return result;
}

} // namespace miniposix

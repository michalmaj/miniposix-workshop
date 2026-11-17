#include "miniposix/pipe.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

    constexpr std::size_t kMib = 1024 * 1024;

    struct Options {
        std::filesystem::path output = "bench-results/pipe.csv";
        std::size_t total_bytes = 8 * kMib;
        std::size_t repeats = 3;
        std::vector<std::size_t> buffer_sizes = {512, 4096, 65536};
        std::size_t pipe_capacity = 0; // 0 = leave the kernel default
        std::size_t warmup = 0;        // transfers discarded before the measured loop
        unsigned int seed = 0;         // 0 = keep the original buffer/repeat order
        bool pattern = false;          // precompute bytes and use memcmp instead of per-byte work
    };

    struct Result {
        std::size_t buffer_bytes;
        std::size_t total_bytes;
        std::size_t repeat;
        double seconds;
        double throughput_mib_s;
    };

    std::size_t positive_integer(std::string_view text, std::string_view option) {
        std::size_t value = 0;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
            throw std::invalid_argument(std::string(option) + " requires a positive integer");
        }
        return value;
    }

    std::size_t nonnegative_integer(std::string_view text, std::string_view option) {
        if (text == "0") {
            return 0;
        }
        return positive_integer(text, option);
    }

    std::vector<std::size_t> parse_buffer_sizes(std::string_view text) {
        std::vector<std::size_t> values;
        while (!text.empty()) {
            const auto separator = text.find(',');
            const auto piece = text.substr(0, separator);
            values.push_back(positive_integer(piece, "--buffer-sizes"));
            if (separator == std::string_view::npos) {
                break;
            }
            text.remove_prefix(separator + 1);
            if (text.empty()) {
                throw std::invalid_argument("--buffer-sizes cannot end with a comma");
            }
        }
        if (values.empty()) {
            throw std::invalid_argument("--buffer-sizes requires a comma-separated list");
        }
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        return values;
    }

    void usage(const char *program) {
        std::cout << "Usage: " << program
                  << " [--output FILE] [--bytes N] [--repeats N] [--buffer-sizes N,N,...]"
                     " [--pipe-capacity N] [--warmup N] [--seed N] [--pattern 0|1]\n"
                  << "Defaults: bench-results/pipe.csv, 8388608 bytes, 3 repeats, 512,4096,65536 byte buffers\n"
                  << "--pipe-capacity 0 leaves the default pipe size; a positive value uses F_SETPIPE_SZ\n"
                  << "--warmup N discards N transfers; --seed N > 0 shuffles the measurement order\n"
                  << "--pattern 1 precomputes the payload and verifies it with memcmp (no per-byte loops)\n";
    }

    Options parse_options(int argc, char **argv) {
        Options options;
        for (int index = 1; index < argc; ++index) {
            const std::string_view option(argv[index]);
            if (option == "--help") {
                usage(argv[0]);
                std::exit(0);
            }
            if (index + 1 == argc) {
                throw std::invalid_argument(std::string(option) + " requires a value");
            }
            const std::string_view value(argv[++index]);
            if (option == "--output") {
                if (value.empty()) {
                    throw std::invalid_argument("--output requires a file path");
                }
                options.output = value;
            } else if (option == "--bytes") {
                options.total_bytes = positive_integer(value, option);
            } else if (option == "--repeats") {
                options.repeats = positive_integer(value, option);
            } else if (option == "--buffer-sizes") {
                options.buffer_sizes = parse_buffer_sizes(value);
            } else if (option == "--pipe-capacity") {
                options.pipe_capacity = nonnegative_integer(value, option);
                if (options.pipe_capacity > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                    throw std::invalid_argument("--pipe-capacity does not fit in an int");
                }
            } else if (option == "--warmup") {
                options.warmup = nonnegative_integer(value, option);
            } else if (option == "--seed") {
                const auto parsed_seed = nonnegative_integer(value, option);
                if (parsed_seed > std::numeric_limits<unsigned int>::max()) {
                    throw std::invalid_argument("--seed must fit in an unsigned int");
                }
                options.seed = static_cast<unsigned int>(parsed_seed);
            } else if (option == "--pattern") {
                if (value != "0" && value != "1") {
                    throw std::invalid_argument("--pattern requires 0 or 1");
                }
                options.pattern = value == "1";
            } else {
                throw std::invalid_argument("unknown option: " + std::string(option));
            }
        }
        return options;
    }

    unsigned char byte_at(std::size_t offset) {
        return static_cast<unsigned char>(offset % 251);
    }

    void write_all(int descriptor, const unsigned char *data, std::size_t count) {
        std::size_t cursor = 0;
        while (cursor < count) {
            const auto written = ::write(descriptor, data + cursor, count - cursor);
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written < 0) {
                throw std::system_error(errno, std::generic_category(), "pipe write");
            }
            if (written == 0) {
                throw std::runtime_error("pipe write made no progress");
            }
            cursor += static_cast<std::size_t>(written);
        }
    }

    // `pattern` is null for the original generated mode; buffers are allocated by measure()
    // before the timer starts so allocation is not part of the measured transfer.
    void write_exactly(int descriptor, std::size_t total_bytes, std::size_t buffer_bytes,
                       std::vector<unsigned char> &buffer, const unsigned char *pattern) {
        std::size_t sent = 0;
        while (sent < total_bytes) {
            const auto count = std::min(buffer_bytes, total_bytes - sent);
            if (pattern != nullptr) {
                write_all(descriptor, pattern + sent, count);
            } else {
                for (std::size_t offset = 0; offset < count; ++offset) {
                    buffer[offset] = byte_at(sent + offset);
                }
                write_all(descriptor, buffer.data(), count);
            }
            sent += count;
        }
    }

    bool read_and_verify(int descriptor, std::size_t total_bytes, std::size_t buffer_bytes,
                         std::vector<unsigned char> &buffer, const unsigned char *pattern) {
        std::size_t received = 0;
        bool correct = true;
        while (received < total_bytes) {
            const auto count = std::min(buffer_bytes, total_bytes - received);
            const auto read_count = ::read(descriptor, buffer.data(), count);
            if (read_count < 0 && errno == EINTR) {
                continue;
            }
            if (read_count < 0) {
                throw std::system_error(errno, std::generic_category(), "pipe read");
            }
            if (read_count == 0) {
                throw std::runtime_error("pipe reached EOF before the expected byte count");
            }
            const auto bytes = static_cast<std::size_t>(read_count);
            if (pattern != nullptr) {
                if (std::memcmp(buffer.data(), pattern + received, bytes) != 0) {
                    correct = false;
                }
            } else {
                for (std::size_t offset = 0; offset < bytes; ++offset) {
                    if (buffer[offset] != byte_at(received + offset)) {
                        correct = false;
                    }
                }
            }
            received += bytes;
        }
        return correct;
    }

    Result measure(std::size_t total_bytes, std::size_t buffer_bytes, std::size_t repeat,
                   std::size_t pipe_capacity, bool use_pattern) {
        auto pipe = miniposix::Pipe::create();
        if (pipe_capacity != 0) {
            const int actual = ::fcntl(pipe.write_end().get(), F_SETPIPE_SZ, static_cast<int>(pipe_capacity));
            if (actual == -1) {
                throw std::system_error(errno, std::generic_category(), "fcntl F_SETPIPE_SZ");
            }
        }
        std::vector<unsigned char> write_buffer(buffer_bytes);
        std::vector<unsigned char> read_buffer(buffer_bytes);
        std::vector<unsigned char> pattern;
        if (use_pattern) {
            pattern.resize(total_bytes);
            for (std::size_t offset = 0; offset < pattern.size(); ++offset) {
                pattern[offset] = byte_at(offset);
            }
        }
        const auto payload = pattern.empty() ? nullptr : pattern.data();
        std::exception_ptr writer_failure;
        std::exception_ptr reader_failure;
        bool correct = false;

        const auto start = std::chrono::steady_clock::now();
        std::jthread writer([&] {
            try {
                write_exactly(pipe.write_end().get(), total_bytes, buffer_bytes, write_buffer, payload);
            } catch (...) {
                writer_failure = std::current_exception();
            }
            pipe.write_end().reset();
        });
        try {
            correct = read_and_verify(pipe.read_end().get(), total_bytes, buffer_bytes, read_buffer, payload);
        } catch (...) {
            reader_failure = std::current_exception();
            // Release the reader so a failed run cannot leave the writer blocked.
            pipe.read_end().reset();
        }
        writer.join();
        const auto end = std::chrono::steady_clock::now();

        if (writer_failure) {
            std::rethrow_exception(writer_failure);
        }
        if (reader_failure) {
            std::rethrow_exception(reader_failure);
        }
        if (!correct) {
            throw std::runtime_error("pipe transferred corrupted data");
        }

        const double seconds = std::chrono::duration<double>(end - start).count();
        if (seconds <= 0) {
            throw std::runtime_error("clock did not advance during measurement");
        }
        return {buffer_bytes, total_bytes, repeat, seconds,
                (static_cast<double>(total_bytes) / static_cast<double>(kMib)) / seconds};
    }

    void save_results(const std::filesystem::path &output, const std::vector<Result> &results) {
        if (!output.parent_path().empty()) {
            std::filesystem::create_directories(output.parent_path());
        }
        const bool needs_header = !std::filesystem::exists(output) || std::filesystem::file_size(output) == 0;
        std::ofstream file(output, std::ios::app);
        if (!file) {
            throw std::runtime_error("cannot open CSV output: " + output.string());
        }
        if (needs_header) {
            file << "variant,buffer_bytes,total_bytes,seconds,throughput_mib_s,repeat\n";
        }
        file << std::setprecision(12);
        for (const auto &result: results) {
            file << "miniposix_pipe," << result.buffer_bytes << ',' << result.total_bytes << ',' << result.seconds << ','
                 << result.throughput_mib_s << ',' << result.repeat << '\n';
        }
        if (!file) {
            throw std::runtime_error("failed to write CSV output: " + output.string());
        }
    }

} // namespace

int main(int argc, char **argv) {
    try {
        const auto options = parse_options(argc, argv);
        // A failed reader closes its end; EPIPE then lets the writer exit and join.
        if (std::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
            throw std::system_error(errno, std::generic_category(), "ignore SIGPIPE");
        }
        for (std::size_t warmup = 0; warmup < options.warmup; ++warmup) {
            for (const auto buffer_bytes: options.buffer_sizes) {
                (void) measure(options.total_bytes, buffer_bytes, 0, options.pipe_capacity, options.pattern);
            }
        }

        struct Task {
            std::size_t buffer_bytes;
            std::size_t repeat;
        };
        std::vector<Task> tasks;
        tasks.reserve(options.buffer_sizes.size() * options.repeats);
        for (const auto buffer_bytes: options.buffer_sizes) {
            for (std::size_t repeat = 1; repeat <= options.repeats; ++repeat) {
                tasks.push_back({buffer_bytes, repeat});
            }
        }
        if (options.seed != 0) {
            std::mt19937 generator(options.seed);
            std::shuffle(tasks.begin(), tasks.end(), generator);
        }

        std::vector<Result> results;
        results.reserve(tasks.size());
        for (const auto &task: tasks) {
            const auto result = measure(options.total_bytes, task.buffer_bytes, task.repeat,
                                        options.pipe_capacity, options.pattern);
            results.push_back(result);
            std::cout << task.buffer_bytes << " bytes, repeat " << task.repeat << ": " << std::fixed
                      << std::setprecision(2) << result.throughput_mib_s << " MiB/s\n";
        }
        save_results(options.output, results);
        std::cout << "Saved " << results.size() << " measurements to " << options.output << '\n';
    } catch (const std::exception &error) {
        std::cerr << "bench_pipe: " << error.what() << '\n';
        return 1;
    }
}

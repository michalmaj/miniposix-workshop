#include "miniposix/pipe.hpp"

#include <gtest/gtest.h>

#include <csignal>
#include <cerrno>
#include <fcntl.h>
#include <sys/resource.h>
#include <system_error>
#include <type_traits>
#include <unistd.h>
#include <utility>

namespace {

bool is_open(int fd) {
    return ::fcntl(fd, F_GETFD) != -1;
}

} // namespace

TEST(Pipe, CannotBeCopiedAndCanBeMoved) {
    static_assert(!std::is_copy_constructible_v<miniposix::Pipe>);
    static_assert(!std::is_copy_assignable_v<miniposix::Pipe>);
    static_assert(std::is_nothrow_move_constructible_v<miniposix::Pipe>);
    static_assert(std::is_nothrow_move_assignable_v<miniposix::Pipe>);
}

TEST(Pipe, TransfersBytesThroughRealDescriptors) {
    auto pipe = miniposix::Pipe::create();
    const int reader = pipe.read_end().get();
    const int writer = pipe.write_end().get();
    ASSERT_GE(reader, 0);
    ASSERT_GE(writer, 0);
    ASSERT_NE(reader, writer);
    ASSERT_TRUE(is_open(reader));
    ASSERT_TRUE(is_open(writer));
    ASSERT_NE(::fcntl(reader, F_SETFL, ::fcntl(reader, F_GETFL) | O_NONBLOCK), -1);

    constexpr char sent = 'P';
    struct sigaction ignored {};
    ignored.sa_handler = SIG_IGN;
    struct sigaction previous {};
    ASSERT_EQ(::sigaction(SIGPIPE, &ignored, &previous), 0);
    const auto written = ::write(writer, &sent, 1);
    ASSERT_EQ(::sigaction(SIGPIPE, &previous, nullptr), 0);
    ASSERT_EQ(written, 1);
    char received = '\0';
    ASSERT_EQ(::read(reader, &received, 1), 1);
    EXPECT_EQ(received, sent);
}

TEST(Pipe, OwnsAndClosesBothEnds) {
    int reader = -1;
    int writer = -1;
    {
        auto pipe = miniposix::Pipe::create();
        reader = pipe.read_end().get();
        writer = pipe.write_end().get();
        ASSERT_GE(reader, 0);
        ASSERT_GE(writer, 0);
        EXPECT_TRUE(is_open(reader));
        EXPECT_TRUE(is_open(writer));
    }

    EXPECT_FALSE(is_open(reader));
    EXPECT_FALSE(is_open(writer));
    if(is_open(reader)) ::close(reader);
    if(is_open(writer)) ::close(writer);
}

TEST(Pipe, ClosingWriterProducesEndOfFile) {
    auto pipe = miniposix::Pipe::create();
    ASSERT_GE(pipe.read_end().get(), 0);
    ASSERT_GE(pipe.write_end().get(), 0);

    pipe.write_end().reset();
    const int reader = pipe.read_end().get();
    ASSERT_NE(::fcntl(reader, F_SETFL, ::fcntl(reader, F_GETFL) | O_NONBLOCK), -1);
    char byte = '\0';
    EXPECT_EQ(::read(reader, &byte, 1), 0);
}

TEST(Pipe, MovingTransfersBothEnds) {
    auto source = miniposix::Pipe::create();
    const int reader = source.read_end().get();
    const int writer = source.write_end().get();
    ASSERT_GE(reader, 0);
    ASSERT_GE(writer, 0);

    auto target = std::move(source);
    EXPECT_EQ(source.read_end().get(), -1);
    EXPECT_EQ(source.write_end().get(), -1);
    EXPECT_EQ(target.read_end().get(), reader);
    EXPECT_EQ(target.write_end().get(), writer);
}

TEST(Pipe, ReportsCreationFailureWithOperatingSystemError) {
    struct rlimit original {};
    ASSERT_EQ(::getrlimit(RLIMIT_NOFILE, &original), 0);
    struct rlimit limited = original;
    limited.rlim_cur = 0;
    ASSERT_EQ(::setrlimit(RLIMIT_NOFILE, &limited), 0);

    bool threw_system_error = false;
    int error_code = 0;
    try {
        auto pipe = miniposix::Pipe::create();
        (void)pipe;
    } catch(const std::system_error& error) {
        threw_system_error = true;
        error_code = error.code().value();
    } catch(...) {
        // Restore the limit before reporting the unexpected exception.
    }

    const int restore_result = ::setrlimit(RLIMIT_NOFILE, &original);
    ASSERT_EQ(restore_result, 0);
    EXPECT_TRUE(threw_system_error);
    if(threw_system_error) EXPECT_EQ(error_code, EMFILE);
}

TEST(Pipe, MoveAssignmentClosesPreviouslyOwnedDescriptors) {
    auto first = miniposix::Pipe::create();
    auto second = miniposix::Pipe::create();
    const int first_reader = first.read_end().get();
    const int first_writer = first.write_end().get();
    const int second_reader = second.read_end().get();
    const int second_writer = second.write_end().get();
    ASSERT_GE(first_reader, 0);
    ASSERT_GE(first_writer, 0);
    ASSERT_GE(second_reader, 0);
    ASSERT_GE(second_writer, 0);

    first = std::move(second);

    EXPECT_FALSE(is_open(first_reader));
    EXPECT_FALSE(is_open(first_writer));
    EXPECT_EQ(first.read_end().get(), second_reader);
    EXPECT_EQ(first.write_end().get(), second_writer);
    EXPECT_EQ(second.read_end().get(), -1);
    EXPECT_EQ(second.write_end().get(), -1);
    EXPECT_TRUE(is_open(second_reader));
    EXPECT_TRUE(is_open(second_writer));

    // Second's former descriptors are still owned by first, so only the old
    // ones may be closed defensively here; first's destructor closes the rest.
    if(is_open(first_reader)) ::close(first_reader);
    if(is_open(first_writer)) ::close(first_writer);
}

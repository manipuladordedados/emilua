// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <boost/predef/os/bsd/free.h>
#include <boost/predef/os/linux.h>
#include <boost/predef/os/unix.h>

#include <sys/stat.h>
#include <algorithm>
#include <cstdint>
#include <string>
#include <array>
#include <map>

#include <unistd.h>

#if !BOOST_OS_UNIX
# error "libc_service only supported on POSIX-like systems"
#endif // !BOOST_OS_UNIX

#define EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE 4

namespace emilua::libc_service {

#if BOOST_OS_LINUX
using thread_id = pid_t;
#elif BOOST_OS_BSD_FREE
using thread_id = long;
#else
using thread_id = std::uintptr_t;
#endif

struct request
{
    enum
    {
        OPEN,
        OPENAT,
        UNLINK,
        RENAME,
        STAT,
        LSTAT,
        ACCESS,
        CONNECT_UNIX,
        CONNECT_INET,
        CONNECT_INET6,
        BIND_UNIX,
        BIND_INET,
        BIND_INET6,
        GETADDRINFO,
    };

    thread_id id;
    int function;
    int intargs[2];

    // Redundant with intargs, but the goal here is to avoid errors (including
    // wrong casts, mixed signedness comparisons, etc). Our goal here is not
    // efficiency, but safety.
    unsigned uintargs[2];

    std::array<char, /*a little less than a pageish=*/4096 - 512> buffer;
};

struct reply
{
    enum
    {
        USE_REPLY_RESULT,
        FORWARD_TO_REAL_LIBC,
    };

    thread_id id;
    int action;
    std::int64_t result;
    int error_code; //< errno

    int intargs[3];
    std::array<
        char,
        std::max<std::size_t>({
            /*IPv6 size=*/16,
            sizeof(struct stat)
        })
    > buffer;
};

void proc_set(int sockfd, std::map<int, std::string> lua_chunk_filters);

} // namespace emilua::libc_service

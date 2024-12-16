// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <boost/predef/os/unix.h>

#if BOOST_OS_UNIX
# include <sys/socket.h>
#endif // BOOST_OS_UNIX

namespace emilua {

#if BOOST_OS_UNIX
extern bool has_libc_service;

struct ambient_authority
{
    int (*open)(int (*)(const char*, int, ...), const char*, int, ...);
    int (*connect)(
        int (*)(int, const struct sockaddr*, socklen_t),
        int, const struct sockaddr*, socklen_t);
    int (*bind)(
        int (*)(int, const struct sockaddr*, socklen_t),
        int, const struct sockaddr*, socklen_t);
};

extern struct ambient_authority ambient_authority;
#endif // BOOST_OS_UNIX

} // namespace emilua

// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <cstdint>

#include <boost/predef/os/unix.h>
#include <boost/predef/os/linux.h>

#if BOOST_OS_UNIX
# include <sys/socket.h>
# include <sys/types.h>
# include <netdb.h>
#endif // BOOST_OS_UNIX

#if BOOST_OS_LINUX
# include <linux/openat2.h>
#endif // BOOST_OS_LINUX

namespace emilua {

#if BOOST_OS_UNIX
extern bool has_libc_service;

struct open_how
{
    enum resolve_flags : std::uint64_t
    {
#if BOOST_OS_LINUX
        resolve_beneath = RESOLVE_BENEATH,
        resolve_in_root = RESOLVE_IN_ROOT,
        resolve_no_magiclinks = RESOLVE_NO_MAGICLINKS,
        resolve_no_symlinks = RESOLVE_NO_SYMLINKS,
        resolve_no_xdev = RESOLVE_NO_XDEV,
        resolve_cached = RESOLVE_CACHED,
#else // BOOST_OS_LINUX
        resolve_beneath       = 1 << 0,
        resolve_in_root       = 1 << 1,
        resolve_no_magiclinks = 1 << 2,
        resolve_no_symlinks   = 1 << 3,
        resolve_no_xdev       = 1 << 4,
        resolve_cached        = 1 << 5,
#endif // BOOST_OS_LINUX

        resolve_mask = resolve_beneath | resolve_in_root |
        resolve_no_magiclinks | resolve_no_symlinks | resolve_no_xdev |
        resolve_cached
    };

    std::uint64_t flags;
    std::uint64_t mode;

    // on 0 (i.e. unused), the interposer should call the old openat() instead
    std::uint64_t resolve;
};

// Glibc provides no wrapper for openat2. However we need this function to be
// interposeable. Therefore we create this wrapper and never call the syscall
// directly. It won't work for 3rd party code, but it'll work for our internals
// at least.
int openat2(int dirfd, const char* pathname, open_how* how);

struct ambient_authority
{
    int (*open)(int (*)(const char*, int, ...), const char*, int, ...);
    int (*unlink)(int (*)(const char*), const char*);
    int (*rename)(int (*)(const char*, const char*), const char*, const char*);
    int (*stat)(int (*)(const char*, struct stat*), const char*, struct stat*);
    int (*lstat)(int (*)(const char*, struct stat*), const char*, struct stat*);
    int (*access)(int (*)(const char*, int), const char*, int);
    int (*eaccess)(int (*)(const char*, int), const char*, int);
    int (*mkdir)(int (*)(const char*, mode_t), const char*, mode_t);
    int (*rmdir)(int (*)(const char*), const char*);
    int (*connect)(
        int (*)(int, const struct sockaddr*, socklen_t),
        int, const struct sockaddr*, socklen_t);
    int (*bind)(
        int (*)(int, const struct sockaddr*, socklen_t),
        int, const struct sockaddr*, socklen_t);
    int (*getaddrinfo)(
        int (*)(
            const char*, const char*, const struct addrinfo*,
            struct addrinfo**),
        const char*, const char*, const struct addrinfo*, struct addrinfo**);

    // also used for old openat (if how.resolve == 0)
    int (*openat2)(
        int (*)(int, const char*, open_how*),
        int, const char*, open_how*);
};

extern struct ambient_authority ambient_authority;
#endif // BOOST_OS_UNIX

} // namespace emilua

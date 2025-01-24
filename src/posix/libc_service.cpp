// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <fcntl.h>
#include <dlfcn.h>

#include <emilua/ambient_authority.hpp>
#include <cstdarg>
#include <cstring>
#include <cerrno>

namespace emilua {
bool has_libc_service = true;
} // namespace emilua

extern "C" {

int open(const char *file, int oflag, ...)
{
    auto real_open = reinterpret_cast<int(*)(const char *file, int oflag, ...)>(
        dlsym(RTLD_NEXT, "open"));

    if (
        ((oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
        ((oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
        false
    ) {
        std::va_list args;
        va_start(args, oflag);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        if (emilua::ambient_authority.open) {
            return (*emilua::ambient_authority.open)(
                real_open, file, oflag, mode);
        } else {
            return real_open(file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.open) {
        return (*emilua::ambient_authority.open)(real_open, file, oflag);
    } else {
        return real_open(file, oflag);
    }
}

static int real_openat2(int dirfd, const char* pathname, emilua::open_how* how)
{
    using emilua::open_how;

    auto real_openat = reinterpret_cast<
        int(*)(int dirfd, const char *file, int oflag, ...)
    >(dlsym(RTLD_NEXT, "openat"));

    // It's not safe to ignore unknown RESOLVE_* flags. That's how openat2() on
    // Linux was born to begin with. On Linux, openat() always ignored unknown
    // bits in its flags argument.
    if (how->resolve != 0) {
        errno = ENOTSUP;
        return -1;
    }

    if (
        ((how->flags & O_CREAT) == O_CREAT) ||
#  ifdef O_TMPFILE
        ((how->flags & O_TMPFILE) == O_TMPFILE) ||
#  endif // defined(O_TMPFILE)
        false
    ) {
        return real_openat(dirfd, pathname, how->flags, how->mode);
    } else {
        return real_openat(dirfd, pathname, how->flags);
    }
}

int openat(int dirfd, const char *file, int oflag, ...)
{
    auto real_openat = reinterpret_cast<
        int(*)(int dirfd, const char *file, int oflag, ...)
    >(dlsym(RTLD_NEXT, "openat"));

    if (
        ((oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
        ((oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
        false
    ) {
        std::va_list args;
        va_start(args, oflag);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        if (emilua::ambient_authority.openat2) {
            emilua::open_how how;
            std::memset(&how, 0, sizeof(how));
            how.flags = oflag;
            how.mode = mode;
            return (*emilua::ambient_authority.openat2)(
                real_openat2, dirfd, file, &how);
        } else {
            return real_openat(dirfd, file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.openat2) {
        emilua::open_how how;
        std::memset(&how, 0, sizeof(how));
        how.flags = oflag;
        return (*emilua::ambient_authority.openat2)(
            real_openat2, dirfd, file, &how);
    } else {
        return real_openat(dirfd, file, oflag);
    }
}

int unlink(const char* pathname)
{
    auto real_unlink = reinterpret_cast<int (*)(const char*)>(
        dlsym(RTLD_NEXT, "unlink"));

    if (emilua::ambient_authority.unlink) {
        return (*emilua::ambient_authority.unlink)(real_unlink, pathname);
    } else {
        return real_unlink(pathname);
    }
}

int connect(int s, const struct sockaddr* name, socklen_t namelen)
{
    auto real_connect = reinterpret_cast<
        int (*)(int, const struct sockaddr*, socklen_t)
    >(dlsym(RTLD_NEXT, "connect"));

    if (emilua::ambient_authority.connect) {
        return (*emilua::ambient_authority.connect)(
            real_connect, s, name, namelen);
    } else {
        return real_connect(s, name, namelen);
    }
}

int bind(int s, const struct sockaddr* name, socklen_t namelen)
{
    auto real_bind = reinterpret_cast<
        int (*)(int, const struct sockaddr*, socklen_t)
    >(dlsym(RTLD_NEXT, "bind"));

    if (emilua::ambient_authority.bind) {
        return (*emilua::ambient_authority.bind)(real_bind, s, name, namelen);
    } else {
        return real_bind(s, name, namelen);
    }
}

int getaddrinfo(
    const char* node, const char* service, const struct addrinfo* hints,
    struct addrinfo** res)
{
    auto real_getaddrinfo = reinterpret_cast<int (*)(
        const char*, const char*, const struct addrinfo*,struct addrinfo**
    )>(dlsym(RTLD_NEXT, "getaddrinfo"));

    if (emilua::ambient_authority.getaddrinfo) {
        return (*emilua::ambient_authority.getaddrinfo)(
            real_getaddrinfo, node, service, hints, res);
    } else {
        return real_getaddrinfo(node, service, hints, res);
    }
}

} // extern "C"

// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <fcntl.h>

#include <emilua/ambient_authority.hpp>
#include <string_view>
#include <unistd.h>
#include <cstdarg>
#include <cerrno>
#include <cstdio>

namespace emilua {
bool has_libc_service = true;
} // namespace emilua

extern "C" {

extern int __sys_open(const char *file, int oflag, ...);
extern int __sys_connect(int, const struct sockaddr*, socklen_t);

int open(const char *file, int oflag, ...)
{
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
                __sys_open, file, oflag, mode);
        } else {
            return __sys_open(file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.open) {
        return (*emilua::ambient_authority.open)(__sys_open, file, oflag);
    } else {
        return __sys_open(file, oflag);
    }
}

FILE* fopen(const char* pathname, const char* mode)
{
    int oflag = 0;
    bool has_mode = false;
    {
        std::string_view mode2{mode};

        if (mode2.starts_with("r+")) {
            oflag |= O_RDWR;
        } else if (mode2.starts_with("r")) {
            oflag |= O_RDONLY;
        } else if (mode2.starts_with("w+x")) {
            oflag |= O_RDWR | O_CREAT | O_TRUNC | O_EXCL;
            has_mode = true;
        } else if (mode2.starts_with("w+")) {
            oflag |= O_RDWR | O_CREAT | O_TRUNC;
            has_mode = true;
        } else if (mode2.starts_with("wx")) {
            oflag |= O_WRONLY | O_CREAT | O_TRUNC | O_EXCL;
            has_mode = true;
        } else if (mode2.starts_with("w")) {
            oflag |= O_WRONLY | O_CREAT | O_TRUNC;
            has_mode = true;
        } else if (mode2.starts_with("a+")) {
            oflag |= O_RDWR | O_CREAT | O_APPEND;
            has_mode = true;
        } else if (mode2.starts_with("a")) {
            oflag |= O_WRONLY | O_CREAT | O_APPEND;
            has_mode = true;
        } else {
            errno = EINVAL;
            return NULL;
        }

        if (mode2.find('e') != std::string_view::npos) {
            oflag |= O_CLOEXEC;
        }
    }

    int fd = has_mode ? open(pathname, oflag, 0666) : open(pathname, oflag);
    if (fd == -1)
        return NULL;

    FILE* ret = fdopen(fd, mode);
    if (ret == NULL) {
        auto last_errno = errno;
        (void)close(fd);
        errno = last_errno;
    }
    return ret;
}

int connect(int s, const struct sockaddr* name, socklen_t namelen)
{
    if (emilua::ambient_authority.connect) {
        return (*emilua::ambient_authority.connect)(
            __sys_connect, s, name, namelen);
    } else {
        return __sys_connect(s, name, namelen);
    }
}

} // extern "C"

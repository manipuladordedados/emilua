// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#define open emilua_doesnt_want_inline_open
#include <fcntl.h>
#undef open

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

extern int __open(const char *file, int oflag, ...);
extern int __open64(const char *file, int oflag, ...);

int open(const char *file, int oflag, ...)
{
    if (((oflag & O_CREAT) == O_CREAT) || ((oflag & O_TMPFILE) == O_TMPFILE)) {
        std::va_list args;
        va_start(args, oflag);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        if (emilua::ambient_authority.open) {
            return (*emilua::ambient_authority.open)(__open, file, oflag, mode);
        } else {
            return __open(file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.open) {
        return (*emilua::ambient_authority.open)(__open, file, oflag);
    } else {
        return __open(file, oflag);
    }
}

int open64(const char *file, int oflag, ...)
{
    oflag |= O_LARGEFILE;
    if (((oflag & O_CREAT) == O_CREAT) || ((oflag & O_TMPFILE) == O_TMPFILE)) {
        std::va_list args;
        va_start(args, oflag);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        if (emilua::ambient_authority.open) {
            return (*emilua::ambient_authority.open)(
                __open64, file, oflag, mode);
        } else {
            return __open64(file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.open) {
        return (*emilua::ambient_authority.open)(__open64, file, oflag);
    } else {
        return __open64(file, oflag);
    }
}

FILE* fopen64(const char* pathname, const char* mode)
{
    int oflag = O_LARGEFILE;
    bool has_mode = false;
    {
        std::string_view mode2{mode};

        if (mode2.starts_with("r+")) {
            oflag |= O_RDWR;
        } else if (mode2.starts_with("r")) {
            oflag |= O_RDONLY;
        } else if (mode2.starts_with("w+")) {
            oflag |= O_RDWR | O_CREAT | O_TRUNC;
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

} // extern "C"

namespace emilua {

FILE* __REDIRECT(fopen, (const char* pathname, const char* mode), fopen);

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
        } else if (mode2.starts_with("w+")) {
            oflag |= O_RDWR | O_CREAT | O_TRUNC;
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

} // namespace emilua

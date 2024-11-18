// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#define open emilua_doesnt_want_inline_open
#include <fcntl.h>
#undef open

#include <emilua/ambient_authority.hpp>
#include <cstdarg>

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

} // extern "C"

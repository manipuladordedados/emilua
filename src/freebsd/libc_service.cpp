// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <fcntl.h>

#include <emilua/ambient_authority.hpp>
#include <cstdarg>

namespace emilua {
bool has_libc_service = true;
} // namespace emilua

extern "C" {

extern int __sys_open(const char *file, int oflag, ...);

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

} // extern "C"

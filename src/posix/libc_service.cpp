// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <fcntl.h>
#include <dlfcn.h>

#include <emilua/ambient_authority.hpp>
#include <cstdarg>

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

} // extern "C"

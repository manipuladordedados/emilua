// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#define open emilua_doesnt_want_inline_open
#include <fcntl.h>
#undef open

#include <emilua/ambient_authority.hpp>
#include <sys/syscall.h>
#include <string_view>
#include <unistd.h>
#include <cstdarg>
#include <cstring>
#include <dlfcn.h>
#include <cerrno>
#include <cstdio>

namespace emilua {
bool has_libc_service = true;
} // namespace emilua

extern "C" {

extern int __open(const char *file, int oflag, ...);
extern int __open64(const char *file, int oflag, ...);
extern int __connect(int, const struct sockaddr*, socklen_t);

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

// Glibc only provides the alias __openat for static builds. On static builds,
// glibc's version will override ours because our symbol is weak. On dynamic
// builds, our version will be used because there are no other definitions for
// this symbol and then we get glibc's implementation through RTLD_NEXT.
[[gnu::weak]]
int __openat(int dirfd, const char *file, int oflag, ...)
{
    auto real_openat = reinterpret_cast<int (*)(
        int dirfd, const char *file, int oflag, ...
    )>(dlsym(RTLD_NEXT, "openat"));

    if (((oflag & O_CREAT) == O_CREAT) || ((oflag & O_TMPFILE) == O_TMPFILE)) {
        std::va_list args;
        va_start(args, oflag);
        mode_t mode = va_arg(args, mode_t);
        va_end(args);
        return real_openat(dirfd, file, oflag, mode);
    } else {
        return real_openat(dirfd, file, oflag);
    }
}

static int real_openat2(int dirfd, const char* pathname, emilua::open_how* how)
{
    if (how->resolve == 0) {
        if (
            ((how->flags & O_CREAT) == O_CREAT) ||
            ((how->flags & O_TMPFILE) == O_TMPFILE)
        ) {
            return __openat(dirfd, pathname, how->flags, how->mode);
        } else {
            return __openat(dirfd, pathname, how->flags);
        }
    } else {
        return syscall(SYS_openat2, dirfd, pathname, how, sizeof(open_how));
    }
}

int openat(int dirfd, const char *file, int oflag, ...)
{
    if (((oflag & O_CREAT) == O_CREAT) || ((oflag & O_TMPFILE) == O_TMPFILE)) {
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
            return __openat(dirfd, file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.openat2) {
        emilua::open_how how;
        std::memset(&how, 0, sizeof(how));
        how.flags = oflag;
        return (*emilua::ambient_authority.openat2)(
            real_openat2, dirfd, file, &how);
    } else {
        return __openat(dirfd, file, oflag);
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

// Glibc only provides the alias __unlink for static builds. On static builds,
// glibc's version will override ours because our symbol is weak. On dynamic
// builds, our version will be used because there are no other definitions for
// this symbol and then we get glibc's implementation through RTLD_NEXT.
[[gnu::weak]]
int __unlink(const char* pathname)
{
    auto real_unlink = reinterpret_cast<int (*)(const char*)>(
        dlsym(RTLD_NEXT, "unlink"));
    return real_unlink(pathname);
}

int unlink(const char* pathname)
{
    if (emilua::ambient_authority.unlink) {
        return (*emilua::ambient_authority.unlink)(__unlink, pathname);
    } else {
        return __unlink(pathname);
    }
}

// Glibc doesn't want us to override remove() so this only works for builds
// against dynamic libc.
[[gnu::weak]]
int remove(const char* pathname)
{
    if (unlink(pathname) == 0)
        return 0;

    if (errno == EISDIR || errno == EPERM)
        return rmdir(pathname);

    return -1;
}

// glibc's rename() is not weak, so we cannot override it when linking
// against static glibc. Defining our own symbol as a weak alias allows builds
// against static glibc to succeed (for such case, our definition won't be
// used).
[[gnu::weak]]
int rename(const char* pathname1, const char* pathname2)
{
    auto real_rename = reinterpret_cast<int (*)(const char*, const char*)>(
        dlsym(RTLD_NEXT, "rename"));

    if (emilua::ambient_authority.rename) {
        return (*emilua::ambient_authority.rename)(
            real_rename, pathname1, pathname2);
    } else {
        return real_rename(pathname1, pathname2);
    }
}

int connect(int s, const struct sockaddr* name, socklen_t namelen)
{
    if (emilua::ambient_authority.connect) {
        return (*emilua::ambient_authority.connect)(
            __connect, s, name, namelen);
    } else {
        return __connect(s, name, namelen);
    }
}

int bind(int s, const struct sockaddr* name, socklen_t namelen)
{
    static constexpr auto real_bind = [](
        int s, const struct sockaddr* name, socklen_t namelen
    ) -> int {
        return syscall(SYS_bind, s, name, namelen);
    };

    if (emilua::ambient_authority.bind) {
        return (*emilua::ambient_authority.bind)(real_bind, s, name, namelen);
    } else {
        return real_bind(s, name, namelen);
    }
}

static int emilua_getaddrinfo(
    const char* node, const char* service, const struct addrinfo* hints,
    struct addrinfo** res)
{
    static constexpr auto real_getaddrinfo = [](
        const char* node, const char* service, const struct addrinfo* hints,
        struct addrinfo** res
    ) -> int {
        struct gaicb cb;
        std::memset(&cb, 0, sizeof(struct gaicb));
        cb.ar_name = node;
        cb.ar_service = service;
        cb.ar_request = hints;
        cb.ar_result = NULL;
        struct gaicb* cbs = &cb;
        auto ret = getaddrinfo_a(GAI_WAIT, &cbs, 1, NULL);
        *res = cb.ar_result;
        return ret;
    };

    if (emilua::ambient_authority.getaddrinfo) {
        return (*emilua::ambient_authority.getaddrinfo)(
            real_getaddrinfo, node, service, hints, res);
    } else {
        return real_getaddrinfo(node, service, hints, res);
    }
}

// glibc's getaddrinfo() is not weak, so we cannot override it when linking
// against static glibc. Defining our own symbol as a weak alias allows builds
// against static glibc to succeed (for such case, our definition won't be
// used).
[[gnu::weak, gnu::alias("emilua_getaddrinfo")]]
int getaddrinfo(
    const char* node, const char* service, const struct addrinfo* hints,
    struct addrinfo** res);

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

int openat2(int dirfd, const char* file, open_how* how)
{
    if (emilua::ambient_authority.openat2) {
        return (*emilua::ambient_authority.openat2)(
            real_openat2, dirfd, file, how);
    } else {
        return real_openat2(dirfd, file, how);
    }
}

} // namespace emilua

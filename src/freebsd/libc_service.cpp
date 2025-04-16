// Copyright (c) 2024, 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <fcntl.h>

#include <emilua/ambient_authority.hpp>
#include <string_view>
#include <dirent.h>
#include <unistd.h>
#include <cstdarg>
#include <dlfcn.h>
#include <cerrno>
#include <cstdio>

namespace emilua {
bool has_libc_service = true;
} // namespace emilua

extern "C" {

extern int __sys_open(const char *file, int oflag, ...);
extern int __sys_openat(int dirfd, const char *file, int oflag, ...);
extern int __sys_unlink(const char *file);
extern int __sys_rename(const char *file1, const char *file2);
extern int __sys_access(const char *file, int amode);
extern int __sys_eaccess(const char *file, int amode);
extern int __sys_mkdir(const char *file, mode_t mode);
extern int __sys_rmdir(const char *file);
extern int __sys_connect(int, const struct sockaddr*, socklen_t);
extern int __sys_bind(int, const struct sockaddr*, socklen_t);
extern int fstatat(int fd, const char *path, struct stat *sb, int flag);

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

static int real_openat2(int dirfd, const char* pathname, emilua::open_how* how)
{
    using emilua::open_how;

    if (
        (how->resolve & open_how::resolve_beneath) == open_how::resolve_beneath
    ) {
        how->resolve &= ~open_how::resolve_beneath;
        how->flags |= O_RESOLVE_BENEATH;
    }

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
        return __sys_openat(dirfd, pathname, how->flags, how->mode);
    } else {
        return __sys_openat(dirfd, pathname, how->flags);
    }
}

int openat(int dirfd, const char *file, int oflag, ...)
{
    emilua::open_how how;
    std::memset(&how, 0, sizeof(how));
    how.flags = oflag;
    if (oflag & O_RESOLVE_BENEATH) {
        how.flags &= ~static_cast<std::uint64_t>(O_RESOLVE_BENEATH);
        how.resolve |= emilua::open_how::resolve_beneath;
    }

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
            how.mode = mode;
            return (*emilua::ambient_authority.openat2)(
                real_openat2, dirfd, file, &how);
        } else {
            return __sys_openat(dirfd, file, oflag, mode);
        }
    }

    if (emilua::ambient_authority.openat2) {
        return (*emilua::ambient_authority.openat2)(
            real_openat2, dirfd, file, &how);
    } else {
        return __sys_openat(dirfd, file, oflag);
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

int unlink(const char* pathname)
{
    if (emilua::ambient_authority.unlink) {
        return (*emilua::ambient_authority.unlink)(__sys_unlink, pathname);
    } else {
        return __sys_unlink(pathname);
    }
}

int remove(const char* pathname)
{
    if (unlink(pathname) == 0)
        return 0;

    if (errno == EISDIR || errno == EPERM)
        return rmdir(pathname);

    return -1;
}

int rename(const char* pathname1, const char* pathname2)
{
    if (emilua::ambient_authority.rename) {
        return (*emilua::ambient_authority.rename)(
            __sys_rename, pathname1, pathname2);
    } else {
        return __sys_rename(pathname1, pathname2);
    }
}

int access(const char* pathname, int amode)
{
    if (emilua::ambient_authority.access) {
        return (*emilua::ambient_authority.access)(
            __sys_access, pathname, amode);
    } else {
        return __sys_access(pathname, amode);
    }
}

int eaccess(const char* pathname, int amode)
{
    if (emilua::ambient_authority.eaccess) {
        return (*emilua::ambient_authority.eaccess)(
            __sys_eaccess, pathname, amode);
    } else {
        return __sys_eaccess(pathname, amode);
    }
}

DIR* opendir(const char* name)
{
    if (name[0] == '\0') {
        errno = ENOENT;
        return NULL;
    }

    int fd = open(name, O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC);
    if (fd == -1) {
        return NULL;
    }

    auto ret = fdopendir(fd);
    if (!ret) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
    }
    return ret;
}

int mkdir(const char* pathname, mode_t mode)
{
    if (emilua::ambient_authority.mkdir) {
        return (*emilua::ambient_authority.mkdir)(__sys_mkdir, pathname, mode);
    } else {
        return __sys_mkdir(pathname, mode);
    }
}

int rmdir(const char* pathname)
{
    if (emilua::ambient_authority.rmdir) {
        return (*emilua::ambient_authority.rmdir)(__sys_rmdir, pathname);
    } else {
        return __sys_rmdir(pathname);
    }
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

int bind(int s, const struct sockaddr* name, socklen_t namelen)
{
    if (emilua::ambient_authority.bind) {
        return (*emilua::ambient_authority.bind)(__sys_bind, s, name, namelen);
    } else {
        return __sys_bind(s, name, namelen);
    }
}

int stat(const char* pathname, struct stat* statbuf)
{
    auto real_stat = [](const char* pathname, struct stat* statbuf) -> int {
        return fstatat(AT_FDCWD, pathname, statbuf, /*flags=*/0);
    };

    if (emilua::ambient_authority.stat) {
        return (*emilua::ambient_authority.stat)(real_stat, pathname, statbuf);
    } else {
        return real_stat(pathname, statbuf);
    }
}

int lstat(const char* pathname, struct stat* statbuf)
{
    auto real_lstat = [](const char* pathname, struct stat* statbuf) -> int {
        return fstatat(AT_FDCWD, pathname, statbuf, AT_SYMLINK_NOFOLLOW);
    };

    if (emilua::ambient_authority.lstat) {
        return (*emilua::ambient_authority.lstat)(
            real_lstat, pathname, statbuf);
    } else {
        return real_lstat(pathname, statbuf);
    }
}

int getaddrinfo(
    const char* node, const char* service, const struct addrinfo* hints,
    struct addrinfo** res)
{
    // libc's doesn't offer an alias for getaddrinfo, so dlsym() is the only way
    // to get one.
    auto real_getaddrinfo = reinterpret_cast<int (*)(
        const char*, const char*, const struct addrinfo*, struct addrinfo**
    )>(dlfunc(RTLD_NEXT, "getaddrinfo"));

    if (emilua::ambient_authority.getaddrinfo) {
        return (*emilua::ambient_authority.getaddrinfo)(
            real_getaddrinfo, node, service, hints, res);
    } else {
        return real_getaddrinfo(node, service, hints, res);
    }
}

} // extern "C"

namespace emilua {

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

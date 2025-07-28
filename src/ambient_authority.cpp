// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/ambient_authority.hpp>
#include <boost/predef/os/bsd/free.h>
#include <boost/predef/os/macos.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

namespace emilua {

#if BOOST_OS_UNIX || BOOST_OS_MACOS
# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
volatile bool has_libc_service = false;

volatile struct ambient_authority ambient_authority;

# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
int openat2(int dirfd, const char* pathname, open_how* how)
{
# if BOOST_OS_LINUX
    if (how->resolve == 0) {
        if (
            ((how->flags & O_CREAT) == O_CREAT) ||
            ((how->flags & O_TMPFILE) == O_TMPFILE)
        ) {
            return openat(dirfd, pathname, how->flags, how->mode);
        } else {
            return openat(dirfd, pathname, how->flags);
        }
    } else {
        return syscall(SYS_openat2, dirfd, pathname, how, sizeof(open_how));
    }
# else
#  if BOOST_OS_BSD_FREE
    if (
        (how->resolve & open_how::resolve_beneath) == open_how::resolve_beneath
    ) {
        how->resolve &= ~open_how::resolve_beneath;
        how->flags |= O_RESOLVE_BENEATH;
    }
#  endif

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
        return openat(dirfd, pathname, how->flags, how->mode);
    } else {
        return openat(dirfd, pathname, how->flags);
    }
# endif
}
#endif // BOOST_OS_UNIX || BOOST_OS_MACOS

} // namespace emilua

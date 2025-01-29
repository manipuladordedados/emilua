// Copyright (c) 2023, 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/detail/landlock.hpp>
#include <emilua/core.hpp>

#include <sys/sysmacros.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>

#include <linux/securebits.h>
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <grp.h>

#include <boost/scope_exit.hpp>

#define EMILUA_DETAIL_INT_CONSTANT(X) \
    [](lua_State* L) -> int {         \
        lua_pushinteger(L, X);        \
        return 1;                     \
    }
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(sys_bindings)
EMILUA_GPERF_NAMESPACE(emilua)
static void check_last_error(lua_State* L, int last_error,
                             const char* perror_string)
{
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror(perror_string);

            // new state which is not memory-limited (LUA_HOOK_BUFFER_SIZE) just
            // to allocate the stacktrace string (we exit the process w/o
            // returning thread's control to the user program so it's fine to
            // skip explicit_bzero())
            lua_State* L2 = luaL_newstate();

            luaL_traceback(L2, L, nullptr, 1);
            fprintf(stderr, "%s\n", lua_tostring(L2, -1));

            std::exit(1);
        }
    }
};

#define CHECK_LAST_ERROR(L, last_error, str) \
    check_last_error(L, last_error, "<3>ipc_actor/init/" str)

EMILUA_GPERF_DECLS_END(sys_bindings)

EMILUA_GPERF_DECLS_BEGIN(landlock)
EMILUA_GPERF_NAMESPACE(emilua)
using detail::landlock_handled_access_fs;
EMILUA_GPERF_DECLS_END(landlock)

int posix_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            return luaL_error(L, "key not found");
        })
        // ### CONSTANTS ###
        EMILUA_GPERF_PAIR("O_CLOEXEC", EMILUA_DETAIL_INT_CONSTANT(O_CLOEXEC))
        // errno
        EMILUA_GPERF_PAIR(
            "EAFNOSUPPORT", EMILUA_DETAIL_INT_CONSTANT(EAFNOSUPPORT))
        EMILUA_GPERF_PAIR("EADDRINUSE", EMILUA_DETAIL_INT_CONSTANT(EADDRINUSE))
        EMILUA_GPERF_PAIR(
            "EADDRNOTAVAIL", EMILUA_DETAIL_INT_CONSTANT(EADDRNOTAVAIL))
        EMILUA_GPERF_PAIR("EISCONN", EMILUA_DETAIL_INT_CONSTANT(EISCONN))
        EMILUA_GPERF_PAIR("E2BIG", EMILUA_DETAIL_INT_CONSTANT(E2BIG))
        EMILUA_GPERF_PAIR("EDOM", EMILUA_DETAIL_INT_CONSTANT(EDOM))
        EMILUA_GPERF_PAIR("EFAULT", EMILUA_DETAIL_INT_CONSTANT(EFAULT))
        EMILUA_GPERF_PAIR("EBADF", EMILUA_DETAIL_INT_CONSTANT(EBADF))
        EMILUA_GPERF_PAIR("EBADMSG", EMILUA_DETAIL_INT_CONSTANT(EBADMSG))
        EMILUA_GPERF_PAIR("EPIPE", EMILUA_DETAIL_INT_CONSTANT(EPIPE))
        EMILUA_GPERF_PAIR(
            "ECONNABORTED", EMILUA_DETAIL_INT_CONSTANT(ECONNABORTED))
        EMILUA_GPERF_PAIR("EALREADY", EMILUA_DETAIL_INT_CONSTANT(EALREADY))
        EMILUA_GPERF_PAIR(
            "ECONNREFUSED", EMILUA_DETAIL_INT_CONSTANT(ECONNREFUSED))
        EMILUA_GPERF_PAIR("ECONNRESET", EMILUA_DETAIL_INT_CONSTANT(ECONNRESET))
        EMILUA_GPERF_PAIR("EXDEV", EMILUA_DETAIL_INT_CONSTANT(EXDEV))
        EMILUA_GPERF_PAIR(
            "EDESTADDRREQ", EMILUA_DETAIL_INT_CONSTANT(EDESTADDRREQ))
        EMILUA_GPERF_PAIR("EBUSY", EMILUA_DETAIL_INT_CONSTANT(EBUSY))
        EMILUA_GPERF_PAIR("ENOTEMPTY", EMILUA_DETAIL_INT_CONSTANT(ENOTEMPTY))
        EMILUA_GPERF_PAIR("ENOEXEC", EMILUA_DETAIL_INT_CONSTANT(ENOEXEC))
        EMILUA_GPERF_PAIR("EEXIST", EMILUA_DETAIL_INT_CONSTANT(EEXIST))
        EMILUA_GPERF_PAIR("EFBIG", EMILUA_DETAIL_INT_CONSTANT(EFBIG))
        EMILUA_GPERF_PAIR(
            "ENAMETOOLONG", EMILUA_DETAIL_INT_CONSTANT(ENAMETOOLONG))
        EMILUA_GPERF_PAIR("ENOSYS", EMILUA_DETAIL_INT_CONSTANT(ENOSYS))
        EMILUA_GPERF_PAIR(
            "EHOSTUNREACH", EMILUA_DETAIL_INT_CONSTANT(EHOSTUNREACH))
        EMILUA_GPERF_PAIR("EIDRM", EMILUA_DETAIL_INT_CONSTANT(EIDRM))
        EMILUA_GPERF_PAIR("EILSEQ", EMILUA_DETAIL_INT_CONSTANT(EILSEQ))
        EMILUA_GPERF_PAIR("ENOTTY", EMILUA_DETAIL_INT_CONSTANT(ENOTTY))
        EMILUA_GPERF_PAIR("EINTR", EMILUA_DETAIL_INT_CONSTANT(EINTR))
        EMILUA_GPERF_PAIR("EINVAL", EMILUA_DETAIL_INT_CONSTANT(EINVAL))
        EMILUA_GPERF_PAIR("ESPIPE", EMILUA_DETAIL_INT_CONSTANT(ESPIPE))
        EMILUA_GPERF_PAIR("EIO", EMILUA_DETAIL_INT_CONSTANT(EIO))
        EMILUA_GPERF_PAIR("EISDIR", EMILUA_DETAIL_INT_CONSTANT(EISDIR))
        EMILUA_GPERF_PAIR("EMSGSIZE", EMILUA_DETAIL_INT_CONSTANT(EMSGSIZE))
        EMILUA_GPERF_PAIR("ENETDOWN", EMILUA_DETAIL_INT_CONSTANT(ENETDOWN))
        EMILUA_GPERF_PAIR("ENETRESET", EMILUA_DETAIL_INT_CONSTANT(ENETRESET))
        EMILUA_GPERF_PAIR(
            "ENETUNREACH", EMILUA_DETAIL_INT_CONSTANT(ENETUNREACH))
        EMILUA_GPERF_PAIR("ENOBUFS", EMILUA_DETAIL_INT_CONSTANT(ENOBUFS))
        EMILUA_GPERF_PAIR("ECHILD", EMILUA_DETAIL_INT_CONSTANT(ECHILD))
        EMILUA_GPERF_PAIR("ENOLINK", EMILUA_DETAIL_INT_CONSTANT(ENOLINK))
        EMILUA_GPERF_PAIR("ENOLCK", EMILUA_DETAIL_INT_CONSTANT(ENOLCK))
        EMILUA_GPERF_PAIR("ENOMSG", EMILUA_DETAIL_INT_CONSTANT(ENOMSG))
        EMILUA_GPERF_PAIR(
            "ENOPROTOOPT", EMILUA_DETAIL_INT_CONSTANT(ENOPROTOOPT))
        EMILUA_GPERF_PAIR("ENOSPC", EMILUA_DETAIL_INT_CONSTANT(ENOSPC))
        EMILUA_GPERF_PAIR("ENXIO", EMILUA_DETAIL_INT_CONSTANT(ENXIO))
        EMILUA_GPERF_PAIR("ENODEV", EMILUA_DETAIL_INT_CONSTANT(ENODEV))
        EMILUA_GPERF_PAIR("ENOENT", EMILUA_DETAIL_INT_CONSTANT(ENOENT))
        EMILUA_GPERF_PAIR("ESRCH", EMILUA_DETAIL_INT_CONSTANT(ESRCH))
        EMILUA_GPERF_PAIR("ENOTDIR", EMILUA_DETAIL_INT_CONSTANT(ENOTDIR))
        EMILUA_GPERF_PAIR("ENOTSOCK", EMILUA_DETAIL_INT_CONSTANT(ENOTSOCK))
        EMILUA_GPERF_PAIR("ENOTCONN", EMILUA_DETAIL_INT_CONSTANT(ENOTCONN))
        EMILUA_GPERF_PAIR("ENOMEM", EMILUA_DETAIL_INT_CONSTANT(ENOMEM))
        EMILUA_GPERF_PAIR("ENOTSUP", EMILUA_DETAIL_INT_CONSTANT(ENOTSUP))
        EMILUA_GPERF_PAIR("ECANCELED", EMILUA_DETAIL_INT_CONSTANT(ECANCELED))
        EMILUA_GPERF_PAIR(
            "EINPROGRESS", EMILUA_DETAIL_INT_CONSTANT(EINPROGRESS))
        EMILUA_GPERF_PAIR("EPERM", EMILUA_DETAIL_INT_CONSTANT(EPERM))
        EMILUA_GPERF_PAIR("EOPNOTSUPP", EMILUA_DETAIL_INT_CONSTANT(EOPNOTSUPP))
        EMILUA_GPERF_PAIR(
            "EWOULDBLOCK", EMILUA_DETAIL_INT_CONSTANT(EWOULDBLOCK))
        EMILUA_GPERF_PAIR("EOWNERDEAD", EMILUA_DETAIL_INT_CONSTANT(EOWNERDEAD))
        EMILUA_GPERF_PAIR("EACCES", EMILUA_DETAIL_INT_CONSTANT(EACCES))
        EMILUA_GPERF_PAIR("EPROTO", EMILUA_DETAIL_INT_CONSTANT(EPROTO))
        EMILUA_GPERF_PAIR(
            "EPROTONOSUPPORT", EMILUA_DETAIL_INT_CONSTANT(EPROTONOSUPPORT))
        EMILUA_GPERF_PAIR("EROFS", EMILUA_DETAIL_INT_CONSTANT(EROFS))
        EMILUA_GPERF_PAIR("EDEADLK", EMILUA_DETAIL_INT_CONSTANT(EDEADLK))
        EMILUA_GPERF_PAIR("EAGAIN", EMILUA_DETAIL_INT_CONSTANT(EAGAIN))
        EMILUA_GPERF_PAIR("ERANGE", EMILUA_DETAIL_INT_CONSTANT(ERANGE))
        EMILUA_GPERF_PAIR(
            "ENOTRECOVERABLE", EMILUA_DETAIL_INT_CONSTANT(ENOTRECOVERABLE))
        EMILUA_GPERF_PAIR("ETXTBSY", EMILUA_DETAIL_INT_CONSTANT(ETXTBSY))
        EMILUA_GPERF_PAIR("ETIMEDOUT", EMILUA_DETAIL_INT_CONSTANT(ETIMEDOUT))
        EMILUA_GPERF_PAIR("ENFILE", EMILUA_DETAIL_INT_CONSTANT(ENFILE))
        EMILUA_GPERF_PAIR("EMFILE", EMILUA_DETAIL_INT_CONSTANT(EMFILE))
        EMILUA_GPERF_PAIR("EMLINK", EMILUA_DETAIL_INT_CONSTANT(EMLINK))
        EMILUA_GPERF_PAIR("ELOOP", EMILUA_DETAIL_INT_CONSTANT(ELOOP))
        EMILUA_GPERF_PAIR("EOVERFLOW", EMILUA_DETAIL_INT_CONSTANT(EOVERFLOW))
        EMILUA_GPERF_PAIR("EPROTOTYPE", EMILUA_DETAIL_INT_CONSTANT(EPROTOTYPE))
        // file creation flags
        EMILUA_GPERF_PAIR("O_CREAT", EMILUA_DETAIL_INT_CONSTANT(O_CREAT))
        EMILUA_GPERF_PAIR("O_RDONLY", EMILUA_DETAIL_INT_CONSTANT(O_RDONLY))
        EMILUA_GPERF_PAIR("O_WRONLY", EMILUA_DETAIL_INT_CONSTANT(O_WRONLY))
        EMILUA_GPERF_PAIR("O_RDWR", EMILUA_DETAIL_INT_CONSTANT(O_RDWR))
        EMILUA_GPERF_PAIR(
            "O_DIRECTORY", EMILUA_DETAIL_INT_CONSTANT(O_DIRECTORY))
        EMILUA_GPERF_PAIR("O_EXCL", EMILUA_DETAIL_INT_CONSTANT(O_EXCL))
        EMILUA_GPERF_PAIR("O_NOCTTY", EMILUA_DETAIL_INT_CONSTANT(O_NOCTTY))
        EMILUA_GPERF_PAIR("O_NOFOLLOW", EMILUA_DETAIL_INT_CONSTANT(O_NOFOLLOW))
        EMILUA_GPERF_PAIR("O_TMPFILE", EMILUA_DETAIL_INT_CONSTANT(O_TMPFILE))
        EMILUA_GPERF_PAIR("O_TRUNC", EMILUA_DETAIL_INT_CONSTANT(O_TRUNC))
        // file status flags
        EMILUA_GPERF_PAIR("O_APPEND", EMILUA_DETAIL_INT_CONSTANT(O_APPEND))
        EMILUA_GPERF_PAIR("O_ASYNC", EMILUA_DETAIL_INT_CONSTANT(O_ASYNC))
        EMILUA_GPERF_PAIR("O_DIRECT", EMILUA_DETAIL_INT_CONSTANT(O_DIRECT))
        EMILUA_GPERF_PAIR("O_DSYNC", EMILUA_DETAIL_INT_CONSTANT(O_DSYNC))
        EMILUA_GPERF_PAIR(
            "O_LARGEFILE", EMILUA_DETAIL_INT_CONSTANT(O_LARGEFILE))
        EMILUA_GPERF_PAIR("O_NOATIME", EMILUA_DETAIL_INT_CONSTANT(O_NOATIME))
        EMILUA_GPERF_PAIR("O_NONBLOCK", EMILUA_DETAIL_INT_CONSTANT(O_NONBLOCK))
        EMILUA_GPERF_PAIR("O_PATH", EMILUA_DETAIL_INT_CONSTANT(O_PATH))
        EMILUA_GPERF_PAIR("O_SYNC", EMILUA_DETAIL_INT_CONSTANT(O_SYNC))
        // mode_t constants
        EMILUA_GPERF_PAIR("S_IRWXU", EMILUA_DETAIL_INT_CONSTANT(S_IRWXU))
        EMILUA_GPERF_PAIR("S_IRUSR", EMILUA_DETAIL_INT_CONSTANT(S_IRUSR))
        EMILUA_GPERF_PAIR("S_IWUSR", EMILUA_DETAIL_INT_CONSTANT(S_IWUSR))
        EMILUA_GPERF_PAIR("S_IXUSR", EMILUA_DETAIL_INT_CONSTANT(S_IXUSR))
        EMILUA_GPERF_PAIR("S_IRWXG", EMILUA_DETAIL_INT_CONSTANT(S_IRWXG))
        EMILUA_GPERF_PAIR("S_IRGRP", EMILUA_DETAIL_INT_CONSTANT(S_IRGRP))
        EMILUA_GPERF_PAIR("S_IWGRP", EMILUA_DETAIL_INT_CONSTANT(S_IWGRP))
        EMILUA_GPERF_PAIR("S_IXGRP", EMILUA_DETAIL_INT_CONSTANT(S_IXGRP))
        EMILUA_GPERF_PAIR("S_IRWXO", EMILUA_DETAIL_INT_CONSTANT(S_IRWXO))
        EMILUA_GPERF_PAIR("S_IROTH", EMILUA_DETAIL_INT_CONSTANT(S_IROTH))
        EMILUA_GPERF_PAIR("S_IWOTH", EMILUA_DETAIL_INT_CONSTANT(S_IWOTH))
        EMILUA_GPERF_PAIR("S_IXOTH", EMILUA_DETAIL_INT_CONSTANT(S_IXOTH))
        EMILUA_GPERF_PAIR("S_ISUID", EMILUA_DETAIL_INT_CONSTANT(S_ISUID))
        EMILUA_GPERF_PAIR("S_ISGID", EMILUA_DETAIL_INT_CONSTANT(S_ISGID))
        EMILUA_GPERF_PAIR("S_ISVTX", EMILUA_DETAIL_INT_CONSTANT(S_ISVTX))
        // access() flags
        EMILUA_GPERF_PAIR("F_OK", EMILUA_DETAIL_INT_CONSTANT(F_OK))
        EMILUA_GPERF_PAIR("R_OK", EMILUA_DETAIL_INT_CONSTANT(R_OK))
        EMILUA_GPERF_PAIR("W_OK", EMILUA_DETAIL_INT_CONSTANT(W_OK))
        EMILUA_GPERF_PAIR("X_OK", EMILUA_DETAIL_INT_CONSTANT(X_OK))
        // openat() flags
        EMILUA_GPERF_PAIR("AT_FDCWD", EMILUA_DETAIL_INT_CONSTANT(AT_FDCWD))
        EMILUA_GPERF_PAIR(
            "AT_EMPTY_PATH", EMILUA_DETAIL_INT_CONSTANT(AT_EMPTY_PATH))
        EMILUA_GPERF_PAIR(
            "AT_SYMLINK_FOLLOW", EMILUA_DETAIL_INT_CONSTANT(AT_SYMLINK_FOLLOW))
        EMILUA_GPERF_PAIR(
            "AT_SYMLINK_NOFOLLOW",
            EMILUA_DETAIL_INT_CONSTANT(AT_SYMLINK_NOFOLLOW))
        // sockets() contants
        EMILUA_GPERF_PAIR("AF_UNIX", EMILUA_DETAIL_INT_CONSTANT(AF_UNIX))
        EMILUA_GPERF_PAIR("AF_LOCAL", EMILUA_DETAIL_INT_CONSTANT(AF_LOCAL))
        EMILUA_GPERF_PAIR("AF_INET", EMILUA_DETAIL_INT_CONSTANT(AF_INET))
        EMILUA_GPERF_PAIR("AF_INET6", EMILUA_DETAIL_INT_CONSTANT(AF_INET6))
        EMILUA_GPERF_PAIR("AF_UNSPEC", EMILUA_DETAIL_INT_CONSTANT(AF_UNSPEC))
        EMILUA_GPERF_PAIR(
            "SOCK_STREAM", EMILUA_DETAIL_INT_CONSTANT(SOCK_STREAM))
        EMILUA_GPERF_PAIR("SOCK_DGRAM", EMILUA_DETAIL_INT_CONSTANT(SOCK_DGRAM))
        EMILUA_GPERF_PAIR(
            "SOCK_SEQPACKET", EMILUA_DETAIL_INT_CONSTANT(SOCK_SEQPACKET))
        EMILUA_GPERF_PAIR(
            "IPPROTO_TCP", EMILUA_DETAIL_INT_CONSTANT(IPPROTO_TCP))
        EMILUA_GPERF_PAIR(
            "IPPROTO_UDP", EMILUA_DETAIL_INT_CONSTANT(IPPROTO_UDP))
        EMILUA_GPERF_PAIR(
            "IPPROTO_SCTP", EMILUA_DETAIL_INT_CONSTANT(IPPROTO_SCTP))
        // listen() constants
        EMILUA_GPERF_PAIR("SOMAXCONN", EMILUA_DETAIL_INT_CONSTANT(SOMAXCONN))
        // mknod() constants
        EMILUA_GPERF_PAIR("S_IFCHR", EMILUA_DETAIL_INT_CONSTANT(S_IFCHR))
        EMILUA_GPERF_PAIR("S_IFBLK", EMILUA_DETAIL_INT_CONSTANT(S_IFBLK))
        // mount() flags
        EMILUA_GPERF_PAIR("MS_REMOUNT", EMILUA_DETAIL_INT_CONSTANT(MS_REMOUNT))
        EMILUA_GPERF_PAIR("MS_BIND", EMILUA_DETAIL_INT_CONSTANT(MS_BIND))
        EMILUA_GPERF_PAIR("MS_SHARED", EMILUA_DETAIL_INT_CONSTANT(MS_SHARED))
        EMILUA_GPERF_PAIR("MS_PRIVATE", EMILUA_DETAIL_INT_CONSTANT(MS_PRIVATE))
        EMILUA_GPERF_PAIR("MS_SLAVE", EMILUA_DETAIL_INT_CONSTANT(MS_SLAVE))
        EMILUA_GPERF_PAIR(
            "MS_UNBINDABLE", EMILUA_DETAIL_INT_CONSTANT(MS_UNBINDABLE))
        EMILUA_GPERF_PAIR("MS_MOVE", EMILUA_DETAIL_INT_CONSTANT(MS_MOVE))
        EMILUA_GPERF_PAIR("MS_DIRSYNC", EMILUA_DETAIL_INT_CONSTANT(MS_DIRSYNC))
        EMILUA_GPERF_PAIR(
            "MS_LAZYTIME", EMILUA_DETAIL_INT_CONSTANT(MS_LAZYTIME))
        EMILUA_GPERF_PAIR(
            "MS_MANDLOCK", EMILUA_DETAIL_INT_CONSTANT(MS_MANDLOCK))
        EMILUA_GPERF_PAIR("MS_NOATIME", EMILUA_DETAIL_INT_CONSTANT(MS_NOATIME))
        EMILUA_GPERF_PAIR("MS_NODEV", EMILUA_DETAIL_INT_CONSTANT(MS_NODEV))
        EMILUA_GPERF_PAIR(
            "MS_NODIRATIME", EMILUA_DETAIL_INT_CONSTANT(MS_NODIRATIME))
        EMILUA_GPERF_PAIR("MS_NOEXEC", EMILUA_DETAIL_INT_CONSTANT(MS_NOEXEC))
        EMILUA_GPERF_PAIR("MS_NOSUID", EMILUA_DETAIL_INT_CONSTANT(MS_NOSUID))
        EMILUA_GPERF_PAIR("MS_RDONLY", EMILUA_DETAIL_INT_CONSTANT(MS_RDONLY))
        EMILUA_GPERF_PAIR("MS_REC", EMILUA_DETAIL_INT_CONSTANT(MS_REC))
        EMILUA_GPERF_PAIR(
            "MS_RELATIME", EMILUA_DETAIL_INT_CONSTANT(MS_RELATIME))
        EMILUA_GPERF_PAIR("MS_SILENT", EMILUA_DETAIL_INT_CONSTANT(MS_SILENT))
        EMILUA_GPERF_PAIR(
            "MS_STRICTATIME", EMILUA_DETAIL_INT_CONSTANT(MS_STRICTATIME))
        EMILUA_GPERF_PAIR(
            "MS_SYNCHRONOUS", EMILUA_DETAIL_INT_CONSTANT(MS_SYNCHRONOUS))
        EMILUA_GPERF_PAIR(
            "MS_NOSYMFOLLOW", EMILUA_DETAIL_INT_CONSTANT(MS_NOSYMFOLLOW))
        // umount2() flags
        EMILUA_GPERF_PAIR("MNT_FORCE", EMILUA_DETAIL_INT_CONSTANT(MNT_FORCE))
        EMILUA_GPERF_PAIR("MNT_DETACH", EMILUA_DETAIL_INT_CONSTANT(MNT_DETACH))
        EMILUA_GPERF_PAIR("MNT_EXPIRE", EMILUA_DETAIL_INT_CONSTANT(MNT_EXPIRE))
        EMILUA_GPERF_PAIR(
            "UMOUNT_NOFOLLOW", EMILUA_DETAIL_INT_CONSTANT(UMOUNT_NOFOLLOW))
        // fsopen() flags
        EMILUA_GPERF_PAIR(
            "FSOPEN_CLOEXEC", EMILUA_DETAIL_INT_CONSTANT(FSOPEN_CLOEXEC))
        // fsconfig() commands
        EMILUA_GPERF_PAIR(
            "FSCONFIG_SET_FLAG", EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_SET_FLAG))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_SET_STRING",
            EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_SET_STRING))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_SET_BINARY",
            EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_SET_BINARY))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_SET_PATH", EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_SET_PATH))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_SET_PATH_EMPTY",
            EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_SET_PATH_EMPTY))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_SET_FD", EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_SET_FD))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_CMD_CREATE",
            EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_CMD_CREATE))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_CMD_RECONFIGURE",
            EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_CMD_RECONFIGURE))
        EMILUA_GPERF_PAIR(
            "FSCONFIG_CMD_CREATE_EXCL",
            EMILUA_DETAIL_INT_CONSTANT(FSCONFIG_CMD_CREATE_EXCL))
        // fsmount() flags
        EMILUA_GPERF_PAIR(
            "FSMOUNT_CLOEXEC", EMILUA_DETAIL_INT_CONSTANT(FSMOUNT_CLOEXEC))
        // move_mount() flags
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_F_SYMLINKS",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_F_SYMLINKS))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_F_AUTOMOUNTS",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_F_AUTOMOUNTS))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_F_EMPTY_PATH",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_F_EMPTY_PATH))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_T_SYMLINKS",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_T_SYMLINKS))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_T_AUTOMOUNTS",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_T_AUTOMOUNTS))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_T_EMPTY_PATH",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_T_EMPTY_PATH))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_SET_GROUP",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_SET_GROUP))
        EMILUA_GPERF_PAIR(
            "MOVE_MOUNT_BENEATH",
            EMILUA_DETAIL_INT_CONSTANT(MOVE_MOUNT_BENEATH))
        // open_tree() flags
        EMILUA_GPERF_PAIR(
            "OPEN_TREE_CLONE", EMILUA_DETAIL_INT_CONSTANT(OPEN_TREE_CLONE))
        EMILUA_GPERF_PAIR(
            "OPEN_TREE_CLOEXEC", EMILUA_DETAIL_INT_CONSTANT(OPEN_TREE_CLOEXEC))
        // fspick() flags
        EMILUA_GPERF_PAIR(
            "FSPICK_CLOEXEC", EMILUA_DETAIL_INT_CONSTANT(FSPICK_CLOEXEC))
        EMILUA_GPERF_PAIR(
            "FSPICK_SYMLINK_NOFOLLOW",
            EMILUA_DETAIL_INT_CONSTANT(FSPICK_SYMLINK_NOFOLLOW))
        EMILUA_GPERF_PAIR(
            "FSPICK_NO_AUTOMOUNT",
            EMILUA_DETAIL_INT_CONSTANT(FSPICK_NO_AUTOMOUNT))
        EMILUA_GPERF_PAIR(
            "FSPICK_EMPTY_PATH", EMILUA_DETAIL_INT_CONSTANT(FSPICK_EMPTY_PATH))
        // mount_setattr() flags
        EMILUA_GPERF_PAIR(
            "AT_RECURSIVE", EMILUA_DETAIL_INT_CONSTANT(AT_RECURSIVE))
        EMILUA_GPERF_PAIR(
            "AT_NO_AUTOMOUNT", EMILUA_DETAIL_INT_CONSTANT(AT_NO_AUTOMOUNT))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_RDONLY", EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_RDONLY))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_NOSUID", EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_NOSUID))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_NODEV", EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_NODEV))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_NOEXEC", EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_NOEXEC))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_NOSYMFOLLOW",
            EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_NOSYMFOLLOW))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_NODIRATIME",
            EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_NODIRATIME))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR__ATIME", EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR__ATIME))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_RELATIME",
            EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_RELATIME))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_NOATIME",
            EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_NOATIME))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_STRICTATIME",
            EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_STRICTATIME))
        EMILUA_GPERF_PAIR(
            "MOUNT_ATTR_IDMAP", EMILUA_DETAIL_INT_CONSTANT(MOUNT_ATTR_IDMAP))
        // setns() flags
        EMILUA_GPERF_PAIR(
            "CLONE_NEWCGROUP", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWCGROUP))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWIPC", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWIPC))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWNET", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWNET))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWNS", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWNS))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWPID", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWPID))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWTIME", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWTIME))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWUSER", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWUSER))
        EMILUA_GPERF_PAIR(
            "CLONE_NEWUTS", EMILUA_DETAIL_INT_CONSTANT(CLONE_NEWUTS))
        // cap_set_secbits() flags
        EMILUA_GPERF_PAIR(
            "SECBIT_NOROOT", EMILUA_DETAIL_INT_CONSTANT(SECBIT_NOROOT))
        EMILUA_GPERF_PAIR(
            "SECBIT_NOROOT_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NOROOT_LOCKED))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_SETUID_FIXUP",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_SETUID_FIXUP))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_SETUID_FIXUP_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_SETUID_FIXUP_LOCKED))
        EMILUA_GPERF_PAIR(
            "SECBIT_KEEP_CAPS", EMILUA_DETAIL_INT_CONSTANT(SECBIT_KEEP_CAPS))
        EMILUA_GPERF_PAIR(
            "SECBIT_KEEP_CAPS_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_KEEP_CAPS_LOCKED))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_CAP_AMBIENT_RAISE",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_CAP_AMBIENT_RAISE))
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED",
            EMILUA_DETAIL_INT_CONSTANT(SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED))
        // ### FUNCTIONS ###
        EMILUA_GPERF_PAIR(
            "dup",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int oldfd = luaL_checkinteger(L, 1);
                    int res = dup(oldfd);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "dup");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "dup2",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int oldfd = luaL_checkinteger(L, 1);
                    int newfd = luaL_checkinteger(L, 2);
                    int res = dup2(oldfd, newfd);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "dup2");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int res = close(fd);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "close");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "closefrom",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int lowfd = luaL_checkinteger(L, 1);
                    if (lowfd < 0) {
                        lowfd = 0;
                    }
                    (void)close_range(lowfd, UINT_MAX, /*flags=*/0);
                    return 0;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int nbyte = luaL_checkinteger(L, 2);
                    void* ud;
                    lua_Alloc a = lua_getallocf(L, &ud);
                    char* buf = static_cast<char*>(a(ud, NULL, 0, nbyte));
                    int res = read(fd, buf, nbyte);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "read");
                    if (last_error == 0) {
                        lua_pushlstring(L, buf, res);
                    } else {
                        lua_pushnil(L);
                    }
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    std::size_t len;
                    const char* str = lua_tolstring(L, 2, &len);
                    int res = write(fd, str, len);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "write");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "open",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 3);
                    const char* str = luaL_checkstring(L, 1);
                    int flags = luaL_checkinteger(L, 2);
                    int res;
                    if (lua_isnil(L, 3)) {
                        res = open(str, flags);
                    } else {
                        mode_t mode = luaL_checkinteger(L, 3);
                        res = open(str, flags, mode);
                    }
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "open");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "access",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    int amode = luaL_checkinteger(L, 2);
                    int res = access(path, amode);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "access");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "eaccess",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    int amode = luaL_checkinteger(L, 2);
                    int res = eaccess(path, amode);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "eaccess");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mkdir",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    mode_t mode = luaL_checkinteger(L, 2);
                    int res = mkdir(path, mode);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "mkdir");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "link",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* oldpath = luaL_checkstring(L, 1);
                    const char* newpath = luaL_checkstring(L, 2);
                    int res = link(oldpath, newpath);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "link");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "linkat",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int olddirfd = luaL_checkinteger(L, 1);
                    const char* oldpath = luaL_checkstring(L, 2);
                    int newdirfd = luaL_checkinteger(L, 3);
                    const char* newpath = luaL_checkstring(L, 4);
                    int flags = luaL_checkinteger(L, 5);
                    int res = linkat(
                        olddirfd, oldpath, newdirfd, newpath, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "linkat");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "symlink",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* target = luaL_checkstring(L, 1);
                    const char* linkpath = luaL_checkstring(L, 2);
                    int res = symlink(target, linkpath);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "symlink");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chown",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    uid_t owner = luaL_checkinteger(L, 2);
                    gid_t group = luaL_checkinteger(L, 3);
                    int res = chown(path, owner, group);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "chown");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chmod",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    mode_t mode = luaL_checkinteger(L, 2);
                    int res = chmod(path, mode);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "chmod");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chdir",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    int res = chdir(path);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "chdir");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umask",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    mode_t res = umask(luaL_checkinteger(L, 1));
                    lua_pushinteger(L, res);
                    return 1;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* source =
                        lua_isnil(L, 1) ? nullptr : lua_tostring(L, 1);
                    const char* target = luaL_checkstring(L, 2);
                    const char* fstype =
                        lua_isnil(L, 3) ? nullptr : lua_tostring(L, 3);
                    unsigned long flags = luaL_checkinteger(L, 4);
                    const void* data =
                        lua_isnil(L, 5) ? nullptr : lua_tostring(L, 5);
                    int res = mount(source, target, fstype, flags, data);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "mount");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* target = luaL_checkstring(L, 1);
                    int res = umount(target);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "umount");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umount2",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* target = luaL_checkstring(L, 1);
                    int flags = luaL_checkinteger(L, 2);
                    int res = umount2(target, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "umount2");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "pivot_root",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* new_root = luaL_checkstring(L, 1);
                    const char* put_old = luaL_checkstring(L, 2);
                    int res = syscall(SYS_pivot_root, new_root, put_old);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "pivot_root");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chroot",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    int res = chroot(path);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "chroot");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mkfifo",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    mode_t mode = luaL_checkinteger(L, 2);
                    int res = mkfifo(path, mode);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "mkfifo");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "socket",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int domain = luaL_checkinteger(L, 1);
                    int type = luaL_checkinteger(L, 2);
                    int protocol = luaL_checkinteger(L, 3);
                    int res = socket(domain, type, protocol);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "socket");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "listen",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int backlog = luaL_checkinteger(L, 2);
                    int res = listen(fd, backlog);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "listen");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mknod",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* path = luaL_checkstring(L, 1);
                    mode_t mode = luaL_checkinteger(L, 2);
                    dev_t dev = luaL_checkinteger(L, 3);
                    int res = mknod(path, mode, dev);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "mknod");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "makedev",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int major = luaL_checkinteger(L, 1);
                    int minor = luaL_checkinteger(L, 2);

                    lua_pushinteger(L, makedev(major, minor));
                    return 1;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "sethostname",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    std::size_t len;
                    const char* str = lua_tolstring(L, 1, &len);
                    int res = sethostname(str, len);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "sethostname");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setdomainname",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    std::size_t len;
                    const char* str = lua_tolstring(L, 1, &len);
                    int res = setdomainname(str, len);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "setdomainname");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setsid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    pid_t res = setsid();
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "setsid");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setpgid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    pid_t pid = luaL_checkinteger(L, 1);
                    pid_t pgid = luaL_checkinteger(L, 2);
                    int res = setpgid(pid, pgid);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "setpgid");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setresuid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    uid_t ruid = luaL_checkinteger(L, 1);
                    uid_t euid = luaL_checkinteger(L, 2);
                    uid_t suid = luaL_checkinteger(L, 3);
                    int res = setresuid(ruid, euid, suid);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "setresuid");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setresgid",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    gid_t rgid = luaL_checkinteger(L, 1);
                    gid_t egid = luaL_checkinteger(L, 2);
                    gid_t sgid = luaL_checkinteger(L, 3);
                    int res = setresgid(rgid, egid, sgid);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "setresgid");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setgroups",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    luaL_checktype(L, 1, LUA_TTABLE);
                    std::vector<gid_t> groups;
                    for (int i = 0 ;; ++i) {
                        lua_rawgeti(L, 1, i + 1);
                        switch (lua_type(L, -1)) {
                        case LUA_TNIL: {
                            int res = setgroups(groups.size(), groups.data());
                            int last_error = (res == -1) ? errno : 0;
                            CHECK_LAST_ERROR(L, last_error, "setgroups");
                            lua_pushinteger(L, res);
                            lua_pushinteger(L, last_error);
                            return 2;
                        }
                        case LUA_TNUMBER:
                            groups.emplace_back(lua_tointeger(L, -1));
                            lua_pop(L, 1);
                            break;
                        default:
                            errno = EINVAL;
                            perror("<3>ipc_actor/init/setgroups");
                            std::exit(1);
                        }
                    }
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_proc",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* caps = luaL_checkstring(L, 1);
                    cap_t caps2 = cap_from_text(caps);
                    if (caps2 == NULL) {
                        perror("<3>ipc_actor/init/cap_set_proc");
                        std::exit(1);
                    }
                    BOOST_SCOPE_EXIT_ALL(&) { cap_free(caps2); };
                    int res = cap_set_proc(caps2);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "cap_set_proc");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_drop_bound",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* name = luaL_checkstring(L, 1);
                    cap_value_t cap;
                    if (cap_from_name(name, &cap) == -1) {
                        perror("<3>ipc_actor/init/cap_drop_bound");
                        std::exit(1);
                    }
                    int res = cap_drop_bound(cap);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "cap_drop_bound");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_ambient",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* name = luaL_checkstring(L, 1);
                    cap_value_t cap;
                    if (cap_from_name(name, &cap) == -1) {
                        perror("<3>ipc_actor/init/cap_set_ambient");
                        std::exit(1);
                    }

                    luaL_checktype(L, 2, LUA_TBOOLEAN);
                    cap_flag_value_t value =
                        lua_toboolean(L, 2) ? CAP_SET : CAP_CLEAR;

                    int res = cap_set_ambient(cap, value);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "cap_set_ambient");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_reset_ambient",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int res = cap_reset_ambient();
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "cap_reset_ambient");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_secbits",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int res = cap_set_secbits(luaL_checkinteger(L, 1));
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "cap_set_secbits");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "unshare",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int nstype = luaL_checkinteger(L, 1);
                    int res = unshare(nstype);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "unshare");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setns",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int nstype = luaL_checkinteger(L, 2);
                    int res = setns(fd, nstype);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "setns");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "execve",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 3);

                    const char* pathname = luaL_checkstring(L, 1);

                    std::vector<std::string> argvbuf;
                    switch (lua_type(L, 2)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        for (int i = 1 ;; ++i) {
                            lua_rawgeti(L, 2, i);
                            switch (lua_type(L, -1)) {
                            case LUA_TNIL:
                                goto end_for;
                            case LUA_TSTRING:
                                argvbuf.emplace_back(tostringview(L));
                                lua_pop(L, 1);
                                break;
                            default:
                                errno = EINVAL;
                                perror("<3>ipc_actor/init/execve");
                                std::exit(1);
                            }
                        }
                    end_for:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/execve");
                        std::exit(1);
                    }

                    std::vector<std::string> envpbuf;
                    switch (lua_type(L, 3)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        lua_pushnil(L);
                        while (lua_next(L, 3) != 0) {
                            if (
                                lua_type(L, -2) != LUA_TSTRING ||
                                lua_type(L, -1) != LUA_TSTRING
                            ) {
                                errno = EINVAL;
                                perror("<3>ipc_actor/init/execve");
                                std::exit(1);
                            }

                            envpbuf.emplace_back(tostringview(L, -2));
                            envpbuf.back() += '=';
                            envpbuf.back() += tostringview(L, -1);
                            lua_pop(L, 1);
                        }
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/execve");
                        std::exit(1);
                    }

                    std::vector<char*> argv;
                    argv.reserve(argvbuf.size() + 1);
                    for (auto& a : argvbuf) {
                        argv.emplace_back(a.data());
                    }
                    argv.emplace_back(nullptr);

                    std::vector<char*> envp;
                    envp.reserve(envpbuf.size() + 1);
                    for (auto& e : envpbuf) {
                        envp.emplace_back(e.data());
                    }
                    envp.emplace_back(nullptr);

                    int res = execve(pathname, argv.data(), envp.data());
                    int last_error = errno;
                    CHECK_LAST_ERROR(L, last_error, "execve");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "fexecve",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 3);

                    int fd = luaL_checkinteger(L, 1);

                    std::vector<std::string> argvbuf;
                    switch (lua_type(L, 2)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        for (int i = 1 ;; ++i) {
                            lua_rawgeti(L, 2, i);
                            switch (lua_type(L, -1)) {
                            case LUA_TNIL:
                                goto end_for;
                            case LUA_TSTRING:
                                argvbuf.emplace_back(tostringview(L));
                                lua_pop(L, 1);
                                break;
                            default:
                                errno = EINVAL;
                                perror("<3>ipc_actor/init/fexecve");
                                std::exit(1);
                            }
                        }
                    end_for:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/fexecve");
                        std::exit(1);
                    }

                    std::vector<std::string> envpbuf;
                    switch (lua_type(L, 3)) {
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        lua_pushnil(L);
                        while (lua_next(L, 3) != 0) {
                            if (
                                lua_type(L, -2) != LUA_TSTRING ||
                                lua_type(L, -1) != LUA_TSTRING
                            ) {
                                errno = EINVAL;
                                perror("<3>ipc_actor/init/fexecve");
                                std::exit(1);
                            }

                            envpbuf.emplace_back(tostringview(L, -2));
                            envpbuf.back() += '=';
                            envpbuf.back() += tostringview(L, -1);
                            lua_pop(L, 1);
                        }
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/fexecve");
                        std::exit(1);
                    }

                    std::vector<char*> argv;
                    argv.reserve(argvbuf.size() + 1);
                    for (auto& a : argvbuf) {
                        argv.emplace_back(a.data());
                    }
                    argv.emplace_back(nullptr);

                    std::vector<char*> envp;
                    envp.reserve(envpbuf.size() + 1);
                    for (auto& e : envpbuf) {
                        envp.emplace_back(e.data());
                    }
                    envp.emplace_back(nullptr);

                    int res = fexecve(fd, argv.data(), envp.data());
                    int last_error = errno;
                    CHECK_LAST_ERROR(L, last_error, "fexecve");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "fsopen",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* pathname;
                    unsigned int flags = luaL_checkinteger(L, 2);

                    switch (lua_type(L, 1)) {
                    case LUA_TSTRING:
                        pathname = lua_tostring(L, 1);
                        break;
                    case LUA_TNIL:
                        pathname = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/fsopen");
                        std::exit(1);
                    }

                    int res = fsopen(pathname, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "fsopen");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "fsmount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    unsigned int flags = luaL_checkinteger(L, 2);
                    unsigned int msflags = luaL_checkinteger(L, 3);

                    int res = fsmount(fd, flags, msflags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "fsmount");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "move_mount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fdirfd = luaL_checkinteger(L, 1);
                    int tdirfd = luaL_checkinteger(L, 3);
                    const char* fpath;
                    const char* tpath;
                    unsigned int flags = luaL_checkinteger(L, 5);

                    switch (lua_type(L, 2)) {
                    case LUA_TSTRING:
                        fpath = lua_tostring(L, 2);
                        break;
                    case LUA_TNIL:
                        fpath = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/move_mount");
                        std::exit(1);
                    }

                    switch (lua_type(L, 4)) {
                    case LUA_TSTRING:
                        tpath = lua_tostring(L, 4);
                        break;
                    case LUA_TNIL:
                        tpath = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/move_mount");
                        std::exit(1);
                    }

                    int res = move_mount(fdirfd, fpath, tdirfd, tpath, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "move_mount");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "fsconfig",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int fd = luaL_checkinteger(L, 1);
                    int cmd = luaL_checkinteger(L, 2);
                    const char* key;
                    const char* value;
                    int aux = luaL_checkinteger(L, 5);

                    switch (lua_type(L, 3)) {
                    case LUA_TSTRING:
                        key = lua_tostring(L, 3);
                        break;
                    case LUA_TNIL:
                        key = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/fsconfig");
                        std::exit(1);
                    }

                    switch (lua_type(L, 4)) {
                    case LUA_TSTRING:
                        value = lua_tostring(L, 4);
                        break;
                    case LUA_TNIL:
                        value = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/fsconfig");
                        std::exit(1);
                    }

                    int res = fsconfig(fd, cmd, key, value, aux);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "fsconfig");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "fspick",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int dirfd = luaL_checkinteger(L, 1);
                    const char* pathname;
                    unsigned int flags = luaL_checkinteger(L, 3);

                    switch (lua_type(L, 2)) {
                    case LUA_TSTRING:
                        pathname = lua_tostring(L, 2);
                        break;
                    case LUA_TNIL:
                        pathname = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/fspick");
                        std::exit(1);
                    }

                    int res = fspick(dirfd, pathname, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "fspick");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "open_tree",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int dirfd = luaL_checkinteger(L, 1);
                    const char* pathname;
                    unsigned int flags = luaL_checkinteger(L, 3);

                    switch (lua_type(L, 2)) {
                    case LUA_TSTRING:
                        pathname = lua_tostring(L, 2);
                        break;
                    case LUA_TNIL:
                        pathname = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/open_tree");
                        std::exit(1);
                    }

                    int res = open_tree(dirfd, pathname, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "open_tree");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mount_setattr",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int dirfd = luaL_checkinteger(L, 1);
                    const char* pathname;
                    unsigned int flags = luaL_checkinteger(L, 3);
                    luaL_checktype(L, 4, LUA_TTABLE);

                    switch (lua_type(L, 2)) {
                    case LUA_TSTRING:
                        pathname = lua_tostring(L, 2);
                        break;
                    case LUA_TNIL:
                        pathname = NULL;
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/mount_setattr");
                        std::exit(1);
                    }

                    struct mount_attr attr;
                    std::memset(&attr, 0, sizeof(attr));

                    lua_pushliteral(L, "attr_set");
                    lua_rawget(L, 4);
                    switch (lua_type(L, -1)) {
                    case LUA_TNUMBER:
                        attr.attr_set = lua_tointeger(L, -1);
                        break;
                    case LUA_TNIL:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/mount_setattr/attr_set");
                        std::exit(1);
                    }
                    lua_pop(L, 1);

                    lua_pushliteral(L, "attr_clr");
                    lua_rawget(L, 4);
                    switch (lua_type(L, -1)) {
                    case LUA_TNUMBER:
                        attr.attr_clr = lua_tointeger(L, -1);
                        break;
                    case LUA_TNIL:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/mount_setattr/attr_clr");
                        std::exit(1);
                    }
                    lua_pop(L, 1);

                    lua_pushliteral(L, "propagation");
                    lua_rawget(L, 4);
                    switch (lua_type(L, -1)) {
                    case LUA_TNUMBER:
                        attr.propagation = lua_tointeger(L, -1);
                        break;
                    case LUA_TNIL:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/mount_setattr/propagation");
                        std::exit(1);
                    }
                    lua_pop(L, 1);

                    lua_pushliteral(L, "userns_fd");
                    lua_rawget(L, 4);
                    switch (lua_type(L, -1)) {
                    case LUA_TNUMBER:
                        attr.userns_fd = lua_tointeger(L, -1);
                        break;
                    case LUA_TNIL:
                        break;
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/mount_setattr/userns_fd");
                        std::exit(1);
                    }
                    lua_pop(L, 1);

                    int res = mount_setattr(
                        dirfd, pathname, flags, &attr, sizeof(attr));
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "mount_setattr");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "seccomp_set_mode_filter",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    std::size_t len;
                    const char* data = lua_tolstring(L, 1, &len);

                    if (len == 0 || len % sizeof(struct sock_filter) != 0) {
                        CHECK_LAST_ERROR(L, EINVAL, "seccomp_set_mode_filter");
                        lua_pushinteger(L, -1);
                        lua_pushinteger(L, EINVAL);
                        return 2;
                    }

                    struct sock_fprog prog;
                    prog.len = len / sizeof(struct sock_filter);

                    if (
                        reinterpret_cast<std::uintptr_t>(data) %
                        alignof(sock_filter) == 0
                    ) {
                        // data already aligned
                        prog.filter = reinterpret_cast<sock_filter*>(
                            const_cast<char*>(data));
                    } else {
                        void* ud;
                        lua_Alloc a = lua_getallocf(L, &ud);
                        prog.filter =
                            static_cast<sock_filter*>(a(ud, NULL, 0, len));
                        std::memcpy(prog.filter, data, len);
                    }

                    int res = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "seccomp_set_mode_filter");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "landlock_create_ruleset",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 2);

                    bool has_ruleset;
                    bool has_flags;

                    switch (lua_type(L, 1)) {
                    case LUA_TTABLE:
                        has_ruleset = true;
                        break;
                    case LUA_TNIL:
                        has_ruleset = false;
                        break;
                    default:
                        return luaL_error(L, "table expected for argument 1");
                    }

                    switch (lua_type(L, 2)) {
                    case LUA_TTABLE:
                        has_flags = true;
                        break;
                    case LUA_TNIL:
                        has_flags = false;
                        break;
                    default:
                        return luaL_error(L, "table expected for argument 2");
                    }

                    struct landlock_ruleset_attr ruleset_attr;
                    std::memset(&ruleset_attr, 0, sizeof(ruleset_attr));

                    if (has_ruleset) {
                        lua_pushnil(L);
                        while (lua_next(L, 1) != 0) {
                            if (lua_type(L, -2) != LUA_TSTRING) {
                                return luaL_error(L, "invalid ruleset attr");
                            }

                            auto hkey = tostringview(L, -2);
                            auto errstr = EMILUA_GPERF_BEGIN(hkey)
                                EMILUA_GPERF_PARAM(
                                    const char* (*action)(
                                        lua_State*, landlock_ruleset_attr&))
                                EMILUA_GPERF_DEFAULT_VALUE(
                                    [](lua_State*, landlock_ruleset_attr&) {
                                        return "invalid ruleset attr";
                                    })
                                EMILUA_GPERF_PAIR(
                                    "handled_access_fs",
                                    [](lua_State* L, landlock_ruleset_attr& a) {
                                        if (lua_type(L, -1) != LUA_TTABLE) {
                                            return "invalid handled_access_fs";
                                        }

                                        auto r = landlock_handled_access_fs(L);
                                        if (r) {
                                            a.handled_access_fs = r.value();
                                            return (const char*)(NULL);
                                        } else {
                                            return r.error();
                                        }
                                    })
                            EMILUA_GPERF_END(hkey)(L, ruleset_attr);
                            if (errstr) {
                                lua_pushstring(L, errstr);
                                return lua_error(L);
                            }
                            lua_pop(L, 1);
                        }
                    }

                    std::uint32_t flags = 0;
                    if (has_flags) {
                        for (int i = 0 ;; ++i) {
                            lua_rawgeti(L, 2, i + 1);
                            switch (lua_type(L, -1)) {
                            case LUA_TNIL: {
                                lua_pop(L, 1);
                                goto end_for;
                            }
                            case LUA_TSTRING:
                                break;
                            default:
                                return luaL_error(
                                    L, "invalid LANDLOCK_CREATE_RULESET flag");
                            }

                            auto fkey = tostringview(L, -1);
                            auto flag = EMILUA_GPERF_BEGIN(fkey)
                                EMILUA_GPERF_PARAM(std::uint32_t action)
                                EMILUA_GPERF_DEFAULT_VALUE(0)
                                EMILUA_GPERF_PAIR("version", 1U << 0)
                            EMILUA_GPERF_END(fkey);
                            if (flag == 0) {
                                return luaL_error(
                                    L, "invalid LANDLOCK_CREATE_RULESET flag");
                            }
                            flags |= flag;
                            lua_pop(L, 1);
                        }
                    end_for:;
                    }

                    int res = syscall(SYS_landlock_create_ruleset,
                                      has_ruleset ? &ruleset_attr : NULL,
                                      has_ruleset ? sizeof(ruleset_attr) : 0,
                                      flags);

                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "landlock_create_ruleset");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "landlock_add_rule",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 4);

                    switch (lua_type(L, 1)) {
                    case LUA_TNUMBER:
                        break;
                    default:
                        return luaL_error(L, "integer expected for argument 1");
                    }

                    if (tostringview(L, 2) != "path_beneath") {
                        return luaL_error(L, "invalid argument 2");
                    }

                    if (lua_type(L, 3) != LUA_TTABLE) {
                        return luaL_error(L, "table expected for argument 3");
                    }

                    switch (lua_type(L, 4)) {
                    case LUA_TNIL:
                        break;
                    default:
                        return luaL_error(L, "nil expected for argument 4");
                    }

                    struct landlock_path_beneath_attr path_beneath_attr;
                    std::memset(
                        &path_beneath_attr, 0, sizeof(path_beneath_attr));
                    path_beneath_attr.parent_fd = -1;

                    lua_pushnil(L);
                    while (lua_next(L, 3) != 0) {
                        if (lua_type(L, -2) != LUA_TSTRING) {
                            return luaL_error(L, "invalid path_beneath attr");
                        }

                        auto hkey = tostringview(L, -2);
                        auto errstr = EMILUA_GPERF_BEGIN(hkey)
                            EMILUA_GPERF_PARAM(
                                const char* (*action)(
                                    lua_State*, landlock_path_beneath_attr&))
                            EMILUA_GPERF_DEFAULT_VALUE(
                                [](lua_State*, landlock_path_beneath_attr&) {
                                    return "invalid path_beneath attr";
                                })
                            EMILUA_GPERF_PAIR(
                                "allowed_access",
                                [](
                                    lua_State* L, landlock_path_beneath_attr& at
                                ) {
                                    if (lua_type(L, -1) != LUA_TTABLE) {
                                        return "invalid allowed_access";
                                    }

                                    auto res = landlock_handled_access_fs(L);
                                    if (res) {
                                        at.allowed_access = res.value();
                                        return (const char*)(NULL);
                                    } else {
                                        return res.error();
                                    }
                                })
                            EMILUA_GPERF_PAIR(
                                "parent_fd",
                                [](
                                    lua_State* L, landlock_path_beneath_attr& at
                                ) {
                                    if (lua_type(L, -1) != LUA_TNUMBER) {
                                        return "invalid parent_fd";
                                    }

                                    at.parent_fd = lua_tointeger(L, -1);
                                    return (const char*)(NULL);
                                })
                        EMILUA_GPERF_END(hkey)(L, path_beneath_attr);
                        if (errstr) {
                            lua_pushstring(L, errstr);
                            return lua_error(L);
                        }
                        lua_pop(L, 1);
                    }

                    int res = syscall(
                        SYS_landlock_add_rule,
                        lua_tointeger(L, 1),
                        /*LANDLOCK_RULE_PATH_BENEATH=*/1,
                        &path_beneath_attr,
                        /*flags=*/0);

                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "landlock_add_rule");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "landlock_restrict_self",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 2);

                    switch (lua_type(L, 1)) {
                    case LUA_TNUMBER:
                        break;
                    default:
                        return luaL_error(L, "integer expected for argument 1");
                    }

                    switch (lua_type(L, 2)) {
                    case LUA_TNIL:
                        break;
                    default:
                        return luaL_error(L, "nil expected for argument 2");
                    }

                    int res = syscall(
                        SYS_landlock_restrict_self, lua_tointeger(L, 1), 0);

                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "landlock_restrict_self");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

} // namespace emilua

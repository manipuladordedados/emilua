EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>

#include <boost/scope_exit.hpp>

#include <sys/capsicum.h>
#include <sys/mount.h>
#include <sys/jail.h>
#include <jail.h>

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
            std::exit(1);
        }
    }
};

#define CHECK_LAST_ERROR(L, last_error, str) \
    check_last_error(L, last_error, "<3>ipc_actor/init/" str)

EMILUA_GPERF_DECLS_END(sys_bindings)

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
        EMILUA_GPERF_PAIR("O_EXEC", EMILUA_DETAIL_INT_CONSTANT(O_EXEC))
        EMILUA_GPERF_PAIR("O_SEARCH", EMILUA_DETAIL_INT_CONSTANT(O_SEARCH))
        EMILUA_GPERF_PAIR(
            "O_DIRECTORY", EMILUA_DETAIL_INT_CONSTANT(O_DIRECTORY))
        EMILUA_GPERF_PAIR("O_EXCL", EMILUA_DETAIL_INT_CONSTANT(O_EXCL))
        EMILUA_GPERF_PAIR("O_NOCTTY", EMILUA_DETAIL_INT_CONSTANT(O_NOCTTY))
        EMILUA_GPERF_PAIR("O_NOFOLLOW", EMILUA_DETAIL_INT_CONSTANT(O_NOFOLLOW))
        EMILUA_GPERF_PAIR("O_TRUNC", EMILUA_DETAIL_INT_CONSTANT(O_TRUNC))
        // file status flags
        EMILUA_GPERF_PAIR("O_APPEND", EMILUA_DETAIL_INT_CONSTANT(O_APPEND))
        EMILUA_GPERF_PAIR("O_ASYNC", EMILUA_DETAIL_INT_CONSTANT(O_ASYNC))
        EMILUA_GPERF_PAIR("O_DIRECT", EMILUA_DETAIL_INT_CONSTANT(O_DIRECT))
        EMILUA_GPERF_PAIR("O_DSYNC", EMILUA_DETAIL_INT_CONSTANT(O_DSYNC))
        EMILUA_GPERF_PAIR("O_NONBLOCK", EMILUA_DETAIL_INT_CONSTANT(O_NONBLOCK))
        EMILUA_GPERF_PAIR(
            "O_RESOLVE_BENEATH", EMILUA_DETAIL_INT_CONSTANT(O_RESOLVE_BENEATH))
        EMILUA_GPERF_PAIR("O_PATH", EMILUA_DETAIL_INT_CONSTANT(O_PATH))
        EMILUA_GPERF_PAIR(
            "O_EMPTY_PATH", EMILUA_DETAIL_INT_CONSTANT(O_EMPTY_PATH))
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
        // openat() flags
        EMILUA_GPERF_PAIR("AT_FDCWD", EMILUA_DETAIL_INT_CONSTANT(AT_FDCWD))
        EMILUA_GPERF_PAIR(
            "AT_EMPTY_PATH", EMILUA_DETAIL_INT_CONSTANT(AT_EMPTY_PATH))
        EMILUA_GPERF_PAIR(
            "AT_SYMLINK_NOFOLLOW",
            EMILUA_DETAIL_INT_CONSTANT(AT_SYMLINK_NOFOLLOW))
        // mknod() constants
        EMILUA_GPERF_PAIR("S_IFCHR", EMILUA_DETAIL_INT_CONSTANT(S_IFCHR))
        EMILUA_GPERF_PAIR("S_IFBLK", EMILUA_DETAIL_INT_CONSTANT(S_IFBLK))
        // mount() flags
        EMILUA_GPERF_PAIR("MNT_RDONLY", EMILUA_DETAIL_INT_CONSTANT(MNT_RDONLY))
        EMILUA_GPERF_PAIR("MNT_NOEXEC", EMILUA_DETAIL_INT_CONSTANT(MNT_NOEXEC))
        EMILUA_GPERF_PAIR("MNT_NOSUID", EMILUA_DETAIL_INT_CONSTANT(MNT_NOSUID))
        EMILUA_GPERF_PAIR(
            "MNT_NOATIME", EMILUA_DETAIL_INT_CONSTANT(MNT_NOATIME))
        EMILUA_GPERF_PAIR(
            "MNT_SNAPSHOT", EMILUA_DETAIL_INT_CONSTANT(MNT_SNAPSHOT))
        EMILUA_GPERF_PAIR(
            "MNT_SUIDDIR", EMILUA_DETAIL_INT_CONSTANT(MNT_SUIDDIR))
        EMILUA_GPERF_PAIR(
            "MNT_SYNCHRONOUS", EMILUA_DETAIL_INT_CONSTANT(MNT_SYNCHRONOUS))
        EMILUA_GPERF_PAIR("MNT_ASYNC", EMILUA_DETAIL_INT_CONSTANT(MNT_ASYNC))
        EMILUA_GPERF_PAIR("MNT_FORCE", EMILUA_DETAIL_INT_CONSTANT(MNT_FORCE))
        EMILUA_GPERF_PAIR(
            "MNT_NOCLUSTERR", EMILUA_DETAIL_INT_CONSTANT(MNT_NOCLUSTERR))
        EMILUA_GPERF_PAIR(
            "MNT_NOCLUSTERW", EMILUA_DETAIL_INT_CONSTANT(MNT_NOCLUSTERW))
        EMILUA_GPERF_PAIR(
            "MNT_NOCOVER", EMILUA_DETAIL_INT_CONSTANT(MNT_NOCOVER))
        EMILUA_GPERF_PAIR(
            "MNT_EMPTYDIR", EMILUA_DETAIL_INT_CONSTANT(MNT_EMPTYDIR))
        EMILUA_GPERF_PAIR("MNT_UPDATE", EMILUA_DETAIL_INT_CONSTANT(MNT_UPDATE))
        EMILUA_GPERF_PAIR("MNT_RELOAD", EMILUA_DETAIL_INT_CONSTANT(MNT_RELOAD))
        EMILUA_GPERF_PAIR("MNT_BYFSID", EMILUA_DETAIL_INT_CONSTANT(MNT_BYFSID))
        // ### FUNCTIONS ###
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
                    const char* fstype = luaL_checkstring(L, 1);
                    const char* target = luaL_checkstring(L, 2);
                    unsigned long flags = luaL_checkinteger(L, 3);
                    void* data = lua_isnil(L, 4) ?
                        nullptr : const_cast<char*>(lua_tostring(L, 4));
                    int res = mount(fstype, target, flags, data);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "mount");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "unmount",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    const char* target = luaL_checkstring(L, 1);
                    int flags = luaL_checkinteger(L, 2);
                    int res = unmount(target, flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "unmount");
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
            "cap_enter",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int res = cap_enter();
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "cap_enter");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "jail_attach",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int jid = luaL_checkinteger(L, 1);
                    int res = jail_attach(jid);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "jail_attach");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "jail_set",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    lua_settop(L, 2);

                    if (lua_type(L, 1) != LUA_TTABLE) {
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/jail_set");
                        std::exit(1);
                    }

                    std::vector<struct jailparam> params;
                    BOOST_SCOPE_EXIT_ALL(&) {
                        jailparam_free(params.data(), params.size());
                    };

                    lua_pushnil(L);
                    while (lua_next(L, 1) != 0) {
                        const char* name;
                        const char* value;

                        if (lua_type(L, -2) != LUA_TSTRING) {
                            errno = EINVAL;
                            perror("<3>ipc_actor/init/jail_set");
                            std::exit(1);
                        }
                        name = lua_tostring(L, -2);

                        switch (lua_type(L, -1)) {
                        default:
                            errno = EINVAL;
                            perror("<3>ipc_actor/init/jail_set");
                            std::exit(1);
                        case LUA_TSTRING:
                            value = lua_tostring(L, -1);
                            break;
                        case LUA_TBOOLEAN:
                            value = (lua_toboolean(L, -1) ? "true" : "false");
                            break;
                        }

                        params.emplace_back();

                        if (jailparam_init(&params.back(), name) == -1) {
                            int last_error = errno;
                            params.pop_back();
                            CHECK_LAST_ERROR(L, last_error, "jail_set");
                            lua_pushinteger(L, -1);
                            lua_pushinteger(L, last_error);
                            return 2;
                        }

                        if (jailparam_import(&params.back(), value) == -1) {
                            int last_error = errno;
                            CHECK_LAST_ERROR(L, last_error, "jail_set");
                            lua_pushinteger(L, -1);
                            lua_pushinteger(L, last_error);
                            return 2;
                        }

                        lua_pop(L, 1);
                    }

                    int flags = 0;

                    switch (lua_type(L, 2)) {
                    default:
                        errno = EINVAL;
                        perror("<3>ipc_actor/init/jail_set");
                        std::exit(1);
                    case LUA_TNIL:
                        break;
                    case LUA_TTABLE:
                        for (int i = 1 ;; ++i) {
                            lua_rawgeti(L, 2, i);
                            switch (lua_type(L, -1)) {
                            default:
                                errno = EINVAL;
                                perror("<3>ipc_actor/init/jail_set");
                                std::exit(1);
                            case LUA_TNIL:
                                lua_pop(L, 1);
                                goto end_for;
                            case LUA_TSTRING:
                                break;
                            }

                            auto s = tostringview(L);
                            lua_pop(L, 1);
                            int f = EMILUA_GPERF_BEGIN(s)
                                EMILUA_GPERF_PARAM(int action)
                                EMILUA_GPERF_DEFAULT_VALUE(0)
                                EMILUA_GPERF_PAIR("create", JAIL_CREATE)
                                EMILUA_GPERF_PAIR("update", JAIL_UPDATE)
                                EMILUA_GPERF_PAIR("attach", JAIL_ATTACH)
                                EMILUA_GPERF_PAIR("dying", JAIL_DYING)
                            EMILUA_GPERF_END(s);
                            if (f == 0) {
                                errno = EINVAL;
                                perror("<3>ipc_actor/init/jail_set");
                                std::exit(1);
                            }
                            flags |= f;
                        }
                    }
                 end_for:

                    int res = jailparam_set(
                        params.data(), params.size(), flags);
                    int last_error = (res == -1) ? errno : 0;
                    CHECK_LAST_ERROR(L, last_error, "jail_set");
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

} // namespace emilua

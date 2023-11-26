EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>

#include <sys/capsicum.h>

#define EMILUA_DETAIL_INT_CONSTANT(X) \
    [](lua_State* L) -> int {         \
        lua_pushinteger(L, X);        \
        return 1;                     \
    }
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(sys_bindings)
static void check_last_error(lua_State* L, int last_error)
{
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror("<3>ipc_actor/init");
            std::exit(1);
        }
    }
};
EMILUA_GPERF_DECLS_END(sys_bindings)

int posix_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            return luaL_error(L, "key not found");
        })
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
        EMILUA_GPERF_PAIR("ENODATA", EMILUA_DETAIL_INT_CONSTANT(ENODATA))
        EMILUA_GPERF_PAIR("ENOMSG", EMILUA_DETAIL_INT_CONSTANT(ENOMSG))
        EMILUA_GPERF_PAIR(
            "ENOPROTOOPT", EMILUA_DETAIL_INT_CONSTANT(ENOPROTOOPT))
        EMILUA_GPERF_PAIR("ENOSPC", EMILUA_DETAIL_INT_CONSTANT(ENOSPC))
        EMILUA_GPERF_PAIR("ENOSR", EMILUA_DETAIL_INT_CONSTANT(ENOSR))
        EMILUA_GPERF_PAIR("ENXIO", EMILUA_DETAIL_INT_CONSTANT(ENXIO))
        EMILUA_GPERF_PAIR("ENODEV", EMILUA_DETAIL_INT_CONSTANT(ENODEV))
        EMILUA_GPERF_PAIR("ENOENT", EMILUA_DETAIL_INT_CONSTANT(ENOENT))
        EMILUA_GPERF_PAIR("ESRCH", EMILUA_DETAIL_INT_CONSTANT(ESRCH))
        EMILUA_GPERF_PAIR("ENOTDIR", EMILUA_DETAIL_INT_CONSTANT(ENOTDIR))
        EMILUA_GPERF_PAIR("ENOTSOCK", EMILUA_DETAIL_INT_CONSTANT(ENOTSOCK))
        EMILUA_GPERF_PAIR("ENOSTR", EMILUA_DETAIL_INT_CONSTANT(ENOSTR))
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
        EMILUA_GPERF_PAIR("ETIME", EMILUA_DETAIL_INT_CONSTANT(ETIME))
        EMILUA_GPERF_PAIR("ETXTBSY", EMILUA_DETAIL_INT_CONSTANT(ETXTBSY))
        EMILUA_GPERF_PAIR("ETIMEDOUT", EMILUA_DETAIL_INT_CONSTANT(ETIMEDOUT))
        EMILUA_GPERF_PAIR("ENFILE", EMILUA_DETAIL_INT_CONSTANT(ENFILE))
        EMILUA_GPERF_PAIR("EMFILE", EMILUA_DETAIL_INT_CONSTANT(EMFILE))
        EMILUA_GPERF_PAIR("EMLINK", EMILUA_DETAIL_INT_CONSTANT(EMLINK))
        EMILUA_GPERF_PAIR("ELOOP", EMILUA_DETAIL_INT_CONSTANT(ELOOP))
        EMILUA_GPERF_PAIR("EOVERFLOW", EMILUA_DETAIL_INT_CONSTANT(EOVERFLOW))
        EMILUA_GPERF_PAIR("EPROTOTYPE", EMILUA_DETAIL_INT_CONSTANT(EPROTOTYPE))
        // ### FUNCTIONS ###
        EMILUA_GPERF_PAIR(
            "cap_enter",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, [](lua_State* L) -> int {
                    int res = cap_enter();
                    int last_error = (res == -1) ? errno : 0;
                    check_last_error(L, last_error);
                    lua_pushinteger(L, res);
                    lua_pushinteger(L, last_error);
                    return 2;
                });
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

} // namespace emilua
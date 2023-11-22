/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/file_descriptor.hpp>

#include <charconv>

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/string.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/div.hpp>

#include <boost/scope_exit.hpp>

#include <boost/predef/os/bsd.h>

#if BOOST_OS_LINUX
#include <sys/capability.h>
#include <emilua/system.hpp>
#endif // BOOST_OS_LINUX

#if BOOST_OS_BSD_FREE
#include <sys/capsicum.h>

#define EMILUA_DETAIL_CAP_CONSTANT(X)      \
    ([]() -> cap_rights_t {                \
        cap_rights_t flag;                 \
        cap_rights_init(&flag, CAP_ ## X); \
        return flag;                       \
    }())
#endif // BOOST_OS_BSD_FREE
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char file_descriptor_mt_key;

EMILUA_GPERF_DECLS_BEGIN(file_descriptor)
EMILUA_GPERF_NAMESPACE(emilua)

#if BOOST_OS_BSD_FREE
static const cap_rights_t empty_rights = []() {
    cap_rights_t ret;
    cap_rights_init(&ret);
    return ret;
}();
#endif // BOOST_OS_BSD_FREE

static int file_descriptor_close(lua_State* L)
{
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 1);

    int res = close(*handle);
    boost::ignore_unused(res);

    return 0;
}

static int file_descriptor_dup(lua_State* L)
{
    auto oldhandle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!oldhandle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*oldhandle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    int newfd = dup(*oldhandle);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (newfd != -1) {
            int res = close(newfd);
            boost::ignore_unused(res);
        }
    };
    if (newfd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    auto newhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *newhandle = newfd;
    newfd = -1;
    return 1;
}

#if BOOST_OS_LINUX
static int file_descriptor_cap_get(lua_State* L)
{
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    cap_t caps = cap_get_fd(*handle);
    if (caps == NULL) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        if (caps != NULL)
            cap_free(caps);
    };

    auto& caps2 = *static_cast<cap_t*>(lua_newuserdata(L, sizeof(cap_t)));
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    setmetatable(L, -2);
    caps2 = caps;
    caps = NULL;

    return 1;
}

static int file_descriptor_cap_set(lua_State* L)
{
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 2));
    if (!caps || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (cap_set_fd(*handle, *caps) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_LINUX

#if BOOST_OS_BSD_FREE
static int file_descriptor_cap_rights_limit(lua_State* L)
{
    lua_settop(L, 2);

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    luaL_checktype(L, 2, LUA_TTABLE);

    cap_rights_t rights;
    cap_rights_init(&rights);

    cap_rights_t all_rights;
    cap_rights_init(&all_rights);
    CAP_ALL(&all_rights);

    for (int i = 0 ;; ++i) {
        lua_rawgeti(L, 2, i + 1);
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            goto input_ready;
        case LUA_TSTRING:
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        auto key = tostringview(L, -1);
        auto flag = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PPGUARD(BOOST_OS_BSD_FREE)
            EMILUA_GPERF_PARAM(cap_rights_t action)
            EMILUA_GPERF_DEFAULT_VALUE(empty_rights)
            EMILUA_GPERF_PAIR("accept", EMILUA_DETAIL_CAP_CONSTANT(ACCEPT))
            EMILUA_GPERF_PAIR(
                "acl_check", EMILUA_DETAIL_CAP_CONSTANT(ACL_CHECK))
            EMILUA_GPERF_PAIR(
                "acl_delete", EMILUA_DETAIL_CAP_CONSTANT(ACL_DELETE))
            EMILUA_GPERF_PAIR("acl_get", EMILUA_DETAIL_CAP_CONSTANT(ACL_GET))
            EMILUA_GPERF_PAIR("acl_set", EMILUA_DETAIL_CAP_CONSTANT(ACL_SET))
            EMILUA_GPERF_PAIR("bind", EMILUA_DETAIL_CAP_CONSTANT(BIND))
            EMILUA_GPERF_PAIR("bindat", EMILUA_DETAIL_CAP_CONSTANT(BINDAT))
            EMILUA_GPERF_PAIR(
                "chflagsat", EMILUA_DETAIL_CAP_CONSTANT(CHFLAGSAT))
            EMILUA_GPERF_PAIR("connect", EMILUA_DETAIL_CAP_CONSTANT(CONNECT))
            EMILUA_GPERF_PAIR(
                "connectat", EMILUA_DETAIL_CAP_CONSTANT(CONNECTAT))
            EMILUA_GPERF_PAIR("create", EMILUA_DETAIL_CAP_CONSTANT(CREATE))
            EMILUA_GPERF_PAIR("event", EMILUA_DETAIL_CAP_CONSTANT(EVENT))
            EMILUA_GPERF_PAIR(
                "extattr_delete", EMILUA_DETAIL_CAP_CONSTANT(EXTATTR_DELETE))
            EMILUA_GPERF_PAIR(
                "cap_extattr_get", EMILUA_DETAIL_CAP_CONSTANT(EXTATTR_GET))
            EMILUA_GPERF_PAIR(
                "cap_extattr_list", EMILUA_DETAIL_CAP_CONSTANT(EXTATTR_LIST))
            EMILUA_GPERF_PAIR(
                "cap_extattr_set", EMILUA_DETAIL_CAP_CONSTANT(EXTATTR_SET))
            EMILUA_GPERF_PAIR("fchdir", EMILUA_DETAIL_CAP_CONSTANT(FCHDIR))
            EMILUA_GPERF_PAIR("fchflags", EMILUA_DETAIL_CAP_CONSTANT(FCHFLAGS))
            EMILUA_GPERF_PAIR("fchmod", EMILUA_DETAIL_CAP_CONSTANT(FCHMOD))
            EMILUA_GPERF_PAIR("fchmodat", EMILUA_DETAIL_CAP_CONSTANT(FCHMODAT))
            EMILUA_GPERF_PAIR("fchown", EMILUA_DETAIL_CAP_CONSTANT(FCHOWN))
            EMILUA_GPERF_PAIR("fchownat", EMILUA_DETAIL_CAP_CONSTANT(FCHOWNAT))
            EMILUA_GPERF_PAIR("fcntl", EMILUA_DETAIL_CAP_CONSTANT(FCNTL))
            EMILUA_GPERF_PAIR("fexecve", EMILUA_DETAIL_CAP_CONSTANT(FEXECVE))
            EMILUA_GPERF_PAIR("flock", EMILUA_DETAIL_CAP_CONSTANT(FLOCK))
            EMILUA_GPERF_PAIR(
                "fpathconf", EMILUA_DETAIL_CAP_CONSTANT(FPATHCONF))
            EMILUA_GPERF_PAIR("fsck", EMILUA_DETAIL_CAP_CONSTANT(FSCK))
            EMILUA_GPERF_PAIR("fstat", EMILUA_DETAIL_CAP_CONSTANT(FSTAT))
            EMILUA_GPERF_PAIR("fstatat", EMILUA_DETAIL_CAP_CONSTANT(FSTATAT))
            EMILUA_GPERF_PAIR("fstatfs", EMILUA_DETAIL_CAP_CONSTANT(FSTATFS))
            EMILUA_GPERF_PAIR("fsync", EMILUA_DETAIL_CAP_CONSTANT(FSYNC))
            EMILUA_GPERF_PAIR(
                "ftruncate", EMILUA_DETAIL_CAP_CONSTANT(FTRUNCATE))
            EMILUA_GPERF_PAIR("futimes", EMILUA_DETAIL_CAP_CONSTANT(FUTIMES))
            EMILUA_GPERF_PAIR(
                "futimesat", EMILUA_DETAIL_CAP_CONSTANT(FUTIMESAT))
            EMILUA_GPERF_PAIR(
                "getpeername", EMILUA_DETAIL_CAP_CONSTANT(GETPEERNAME))
            EMILUA_GPERF_PAIR(
                "getsockname", EMILUA_DETAIL_CAP_CONSTANT(GETSOCKNAME))
            EMILUA_GPERF_PAIR(
                "getsockopt", EMILUA_DETAIL_CAP_CONSTANT(GETSOCKOPT))
            EMILUA_GPERF_PAIR("ioctl", EMILUA_DETAIL_CAP_CONSTANT(IOCTL))
            EMILUA_GPERF_PAIR("kqueue", EMILUA_DETAIL_CAP_CONSTANT(KQUEUE))
            EMILUA_GPERF_PAIR(
                "kqueue_change", EMILUA_DETAIL_CAP_CONSTANT(KQUEUE_CHANGE))
            EMILUA_GPERF_PAIR(
                "kqueue_event", EMILUA_DETAIL_CAP_CONSTANT(KQUEUE_EVENT))
            EMILUA_GPERF_PAIR(
                "linkat_source", EMILUA_DETAIL_CAP_CONSTANT(LINKAT_SOURCE))
            EMILUA_GPERF_PAIR(
                "linkat_target", EMILUA_DETAIL_CAP_CONSTANT(LINKAT_TARGET))
            EMILUA_GPERF_PAIR("listen", EMILUA_DETAIL_CAP_CONSTANT(LISTEN))
            EMILUA_GPERF_PAIR("lookup", EMILUA_DETAIL_CAP_CONSTANT(LOOKUP))
            EMILUA_GPERF_PAIR("mac_get", EMILUA_DETAIL_CAP_CONSTANT(MAC_GET))
            EMILUA_GPERF_PAIR("mac_set", EMILUA_DETAIL_CAP_CONSTANT(MAC_SET))
            EMILUA_GPERF_PAIR("mkdirat", EMILUA_DETAIL_CAP_CONSTANT(MKDIRAT))
            EMILUA_GPERF_PAIR("mkfifoat", EMILUA_DETAIL_CAP_CONSTANT(MKFIFOAT))
            EMILUA_GPERF_PAIR("mknodat", EMILUA_DETAIL_CAP_CONSTANT(MKNODAT))
            EMILUA_GPERF_PAIR("mmap", EMILUA_DETAIL_CAP_CONSTANT(MMAP))
            EMILUA_GPERF_PAIR("mmap_r", EMILUA_DETAIL_CAP_CONSTANT(MMAP_R))
            EMILUA_GPERF_PAIR("mmap_rw", EMILUA_DETAIL_CAP_CONSTANT(MMAP_RW))
            EMILUA_GPERF_PAIR("mmap_rwx", EMILUA_DETAIL_CAP_CONSTANT(MMAP_RWX))
            EMILUA_GPERF_PAIR("mmap_rx", EMILUA_DETAIL_CAP_CONSTANT(MMAP_RX))
            EMILUA_GPERF_PAIR("mmap_w", EMILUA_DETAIL_CAP_CONSTANT(MMAP_W))
            EMILUA_GPERF_PAIR("mmap_wx", EMILUA_DETAIL_CAP_CONSTANT(MMAP_WX))
            EMILUA_GPERF_PAIR("mmap_x", EMILUA_DETAIL_CAP_CONSTANT(MMAP_X))
            EMILUA_GPERF_PAIR("pdgetpid", EMILUA_DETAIL_CAP_CONSTANT(PDGETPID))
            EMILUA_GPERF_PAIR("pdkill", EMILUA_DETAIL_CAP_CONSTANT(PDKILL))
            EMILUA_GPERF_PAIR("peeloff", EMILUA_DETAIL_CAP_CONSTANT(PEELOFF))
            EMILUA_GPERF_PAIR("pread", EMILUA_DETAIL_CAP_CONSTANT(PREAD))
            EMILUA_GPERF_PAIR("pwrite", EMILUA_DETAIL_CAP_CONSTANT(PWRITE))
            EMILUA_GPERF_PAIR("read", EMILUA_DETAIL_CAP_CONSTANT(READ))
            EMILUA_GPERF_PAIR("recv", EMILUA_DETAIL_CAP_CONSTANT(RECV))
            EMILUA_GPERF_PAIR(
                "renameat_source", EMILUA_DETAIL_CAP_CONSTANT(RENAMEAT_SOURCE))
            EMILUA_GPERF_PAIR(
                "renameat_target", EMILUA_DETAIL_CAP_CONSTANT(RENAMEAT_TARGET))
            EMILUA_GPERF_PAIR("seek", EMILUA_DETAIL_CAP_CONSTANT(SEEK))
            EMILUA_GPERF_PAIR(
                "sem_getvalue", EMILUA_DETAIL_CAP_CONSTANT(SEM_GETVALUE))
            EMILUA_GPERF_PAIR("sem_post", EMILUA_DETAIL_CAP_CONSTANT(SEM_POST))
            EMILUA_GPERF_PAIR("sem_wait", EMILUA_DETAIL_CAP_CONSTANT(SEM_WAIT))
            EMILUA_GPERF_PAIR("send", EMILUA_DETAIL_CAP_CONSTANT(SEND))
            EMILUA_GPERF_PAIR(
                "setsockopt", EMILUA_DETAIL_CAP_CONSTANT(SETSOCKOPT))
            EMILUA_GPERF_PAIR("shutdown", EMILUA_DETAIL_CAP_CONSTANT(SHUTDOWN))
            EMILUA_GPERF_PAIR(
                "symlinkat", EMILUA_DETAIL_CAP_CONSTANT(SYMLINKAT))
            EMILUA_GPERF_PAIR("ttyhook", EMILUA_DETAIL_CAP_CONSTANT(TTYHOOK))
            EMILUA_GPERF_PAIR("unlinkat", EMILUA_DETAIL_CAP_CONSTANT(UNLINKAT))
            EMILUA_GPERF_PAIR("write", EMILUA_DETAIL_CAP_CONSTANT(WRITE))
        EMILUA_GPERF_END(key);

        {
            cap_rights_t complement = all_rights;
            cap_rights_remove(&complement, &flag);
            if (cap_rights_contains(&complement, &all_rights)) {
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            }
        }

        cap_rights_set(&rights, flag);
        lua_pop(L, 1);
    }

 input_ready:
    if (cap_rights_limit(*handle, &rights) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

static int file_descriptor_cap_ioctls_limit(lua_State* L)
{
    lua_settop(L, 2);

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<unsigned long> cmds;
    for (int i = 0 ;; ++i) {
        lua_rawgeti(L, 2, i + 1);
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            goto input_ready;
        case LUA_TNUMBER:
            cmds.emplace_back(lua_tointeger(L, -1));
            lua_pop(L, 1);
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
    }

 input_ready:
    if (cap_ioctls_limit(*handle, cmds.data(), cmds.size()) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

static int file_descriptor_cap_fcntls_limit(lua_State* L)
{
    lua_settop(L, 2);

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    luaL_checktype(L, 2, LUA_TTABLE);
    std::uint32_t fcntlrights = 0;
    for (int i = 0 ;; ++i) {
        lua_rawgeti(L, 2, i + 1);
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            goto input_ready;
        case LUA_TSTRING:
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        auto key = tostringview(L, -1);
        auto flag = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PPGUARD(BOOST_OS_BSD_FREE)
            EMILUA_GPERF_PARAM(std::uint32_t action)
            EMILUA_GPERF_DEFAULT_VALUE(0)
            EMILUA_GPERF_PAIR("getfl", CAP_FCNTL_GETFL)
            EMILUA_GPERF_PAIR("setfl", CAP_FCNTL_SETFL)
            EMILUA_GPERF_PAIR("getown", CAP_FCNTL_GETOWN)
            EMILUA_GPERF_PAIR("setown", CAP_FCNTL_SETOWN)
        EMILUA_GPERF_END(key);
        if (flag == 0) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        fcntlrights |= flag;
        lua_pop(L, 1);
    }

 input_ready:
    if (cap_fcntls_limit(*handle, fcntlrights) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}
#endif // BOOST_OS_BSD_FREE
EMILUA_GPERF_DECLS_END(file_descriptor)

static int file_descriptor_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_descriptor_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "dup",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_descriptor_dup);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_get",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, file_descriptor_cap_get);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, file_descriptor_cap_set);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_rights_limit",
            [](lua_State* L) -> int {
#if BOOST_OS_BSD_FREE
                lua_pushcfunction(L, file_descriptor_cap_rights_limit);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_BSD_FREE
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_ioctls_limit",
            [](lua_State* L) -> int {
#if BOOST_OS_BSD_FREE
                lua_pushcfunction(L, file_descriptor_cap_ioctls_limit);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_BSD_FREE
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_fcntls_limit",
            [](lua_State* L) -> int {
#if BOOST_OS_BSD_FREE
                lua_pushcfunction(L, file_descriptor_cap_fcntls_limit);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_BSD_FREE
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int file_descriptor_mt_tostring(lua_State* L)
{
    auto& handle = *static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));

    if (handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    // Paranoia. Apparently the kernel side disagrees about what should be the
    // file descriptor's underlying type (cf. close_range(2)) so negative values
    // could be possible (very unlikely as EMFILE should still hinder them
    // anyway).
    if (handle < 0) [[unlikely]] {
        lua_pushfstring(L, "/dev/fd/%i", handle);
        return 1;
    }

    auto prefix = BOOST_HANA_STRING("/dev/fd/");
    constexpr auto max_digits = hana::second(hana::while_(
        [](auto s) { return hana::first(s) > hana::int_c<0>; },
        hana::make_pair(
            /*i=*/hana::int_c<std::numeric_limits<int>::max()>,
            /*max_digits=*/hana::int_c<0>),
        [](auto s) {
            return hana::make_pair(
                hana::first(s) / hana::int_c<10>,
                hana::second(s) + hana::int_c<1>);
        }
    ));

    std::array<char, hana::length(prefix).value + max_digits.value> buf;
    std::memcpy(buf.data(), prefix.c_str(), hana::length(prefix));
    auto s_size = std::to_chars(
        buf.data() + hana::length(prefix).value,
        buf.data() + buf.size(),
        handle).ptr - buf.data();

    lua_pushlstring(L, buf.data(), s_size);
    return 1;
}

static int file_descriptor_mt_gc(lua_State* L)
{
    auto& handle = *static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (handle == INVALID_FILE_DESCRIPTOR)
        return 0;

    int res = close(handle);
    boost::ignore_unused(res);
    return 0;
}

void init_file_descriptor(lua_State* L)
{
    lua_pushlightuserdata(L, &file_descriptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file_descriptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_descriptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, file_descriptor_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, file_descriptor_mt_gc);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

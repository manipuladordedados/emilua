// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

// This implementation is also used in libemilua-preload-libc. Processes where
// libemilua-preload-libc is injected through LD_PRELOAD shoulnd't be modified
// to use the Emilua runtime. Therefore the implementation of this file remains
// largely neutral and only depends on standard system libraries such as the
// C/C++ runtime.
//
// This file also makes use of TEMP_FAILURE_RETRY. Given this file assumes we
// have no control over the runtime, we can't assume SA_RESTART.

// These headers are largely independent from the rest of Emilua and don't
// include non-standard headers. If/when they do include non-standard headers,
// it's just headers for macro-only libraries such as Boost.Predef and
// Boost.PP. {{{
#include <emilua/proc_set_libc_service.hpp>
#include <emilua/ambient_authority.hpp>
#include <emilua/open_posix_libs.hpp>
// }}}

// Not too intrusive/opinionated header-only libaries are okay too. {{{
#include <boost/endian/conversion.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/scope_exit.hpp>
// }}}

#include <condition_variable>
#include <unordered_map>
#include <forward_list>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <charconv>
#include <sys/un.h>
#include <cstdarg>
#include <cassert>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <span>

extern "C" {
#include <lauxlib.h>
#include <luajit.h>
#include <lualib.h>
#include <lua.h>
}

#if BOOST_OS_BSD_FREE
#include <sys/thr.h>
#endif // BOOST_OS_BSD_FREE

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(X) \
    (([&]() { \
        auto ret = (X); \
        while (ret == -1 && errno == EINTR) { \
            ret = (X); \
        } \
        return ret; \
    })())
#endif // TEMP_FAILURE_RETRY

namespace emilua::libc_service {

using fds_type = std::array<int, EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE>;

template<class T>
using pool_allocator = boost::fast_pool_allocator<T>;

template<class T>
struct pool_ptr_deleter
{
    void operator()(T* p) { pool_allocator<T>::deallocate(p); }
};

template<class T>
using pool_ptr = std::unique_ptr<T, pool_ptr_deleter<T>>;

using request_ptr = pool_ptr<struct request>;

struct reply_with_metadata : public reply
{
    bool push_fd(int fd)
    {
        for (int& o : fds) {
            if (o != -1)
                continue;

            o = fd;
            return true;
        }
        return false;
    }

    fds_type fds;
};

using reply_with_metadata_ptr = pool_ptr<reply_with_metadata>;

namespace {

struct lua_filter
{
    lua_filter()
    {
        L = luaL_newstate();
        if (!L)
            throw std::bad_alloc{};

        luaL_openlibs(L);

        open_posix_libs(L);
        lua_pushboolean(L, 0);
        lua_setglobal(L, "errexit");

        if (filters.contains(request::OPEN)) {
            const auto& src = filters[request::OPEN];

            lua_pushlightuserdata(L, &open_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::OPENAT)) {
            const auto& src = filters[request::OPENAT];

            lua_pushlightuserdata(L, &openat_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::UNLINK)) {
            const auto& src = filters[request::UNLINK];

            lua_pushlightuserdata(L, &unlink_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::RENAME)) {
            const auto& src = filters[request::RENAME];

            lua_pushlightuserdata(L, &rename_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::CONNECT_UNIX)) {
            const auto& src = filters[request::CONNECT_UNIX];

            lua_pushlightuserdata(L, &connect_unix_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::CONNECT_INET)) {
            const auto& src = filters[request::CONNECT_INET];

            lua_pushlightuserdata(L, &connect_inet_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::CONNECT_INET6)) {
            const auto& src = filters[request::CONNECT_INET6];

            lua_pushlightuserdata(L, &connect_inet6_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::BIND_UNIX)) {
            const auto& src = filters[request::BIND_UNIX];

            lua_pushlightuserdata(L, &bind_unix_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::BIND_INET)) {
            const auto& src = filters[request::BIND_INET];

            lua_pushlightuserdata(L, &bind_inet_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::BIND_INET6)) {
            const auto& src = filters[request::BIND_INET6];

            lua_pushlightuserdata(L, &bind_inet6_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }

        if (filters.contains(request::GETADDRINFO)) {
            const auto& src = filters[request::GETADDRINFO];

            lua_pushlightuserdata(L, &getaddrinfo_key);
            switch (luaL_loadbuffer(L, src.data(), src.size(), NULL)) {
            case 0:
                lua_rawset(L, LUA_REGISTRYINDEX);
                break;
            default:
                lua_pop(L, 2);
            }
        }
    }

    ~lua_filter()
    {
        lua_close(L);
    }

    lua_State* L;

    static std::map<int, std::string> filters;
    static char open_key;
    static char openat_key;
    static char unlink_key;
    static char rename_key;
    static char connect_unix_key;
    static char connect_inet_key;
    static char connect_inet6_key;
    static char bind_unix_key;
    static char bind_inet_key;
    static char bind_inet6_key;
    static char getaddrinfo_key;
};

std::map<int, std::string> lua_filter::filters;
char lua_filter::open_key;
char lua_filter::openat_key;
char lua_filter::unlink_key;
char lua_filter::rename_key;
char lua_filter::connect_unix_key;
char lua_filter::connect_inet_key;
char lua_filter::connect_inet6_key;
char lua_filter::bind_unix_key;
char lua_filter::bind_inet_key;
char lua_filter::bind_inet6_key;
char lua_filter::getaddrinfo_key;

struct lua_filter_ptr
{
    lua_filter_ptr()
    {
        {
            [[maybe_unused]] std::lock_guard lk{pool_mtx};
            if (!pool.empty()) {
                box.splice_after(box.before_begin(), pool, pool.before_begin());
                return;
            }
        }
        box.emplace_front();
    }

    ~lua_filter_ptr()
    {
        [[maybe_unused]] std::lock_guard lk{pool_mtx};
        pool.splice_after(pool.before_begin(), box);
    }

    lua_filter_ptr(lua_filter_ptr&&) = default;
    lua_filter_ptr& operator=(lua_filter_ptr&&) = default;
    lua_filter_ptr(const lua_filter_ptr&) = delete;
    lua_filter_ptr& operator=(const lua_filter_ptr&) = delete;

    lua_filter& operator*()
    {
        return *box.begin();
    }

    lua_filter* operator->()
    {
        return &*box.begin();
    }

    std::forward_list<lua_filter> box;

    static std::forward_list<lua_filter> pool;
    static std::mutex pool_mtx;
};

std::forward_list<lua_filter> lua_filter_ptr::pool;
std::mutex lua_filter_ptr::pool_mtx;

} // namespace

#if !BOOST_OS_LINUX && !BOOST_OS_BSD_FREE
// I don't trust std::this_thread::get_id() for pthread_create() threads nor
// pthread_self() for std::thread() threads (we don't control in which threads
// our code is called from). Hence this homemade implementation for
// get-thread-id.
//
// My distrust first arose from the comments on this bug report:
// <https://bugzilla.kernel.org/show_bug.cgi?id=218607>.
static thread_local char cookie_for_thread_id;
#endif // !BOOST_OS_LINUX && !BOOST_OS_BSD_FREE

static int sockfd = -1;

// TODO: Use containers from Boost.Intrusive to have more control over
// allocations (and minimize them).
static std::unordered_map<thread_id, reply_with_metadata_ptr> receive_queue;
static bool reading = false;
static std::mutex receive_queue_mtx;

// Either a new reply has arrived or there's no longer a reader (`reading`
// became falsy). Either way the waiters must take action.
static std::condition_variable receive_queue_changed_cond;

static inline request_ptr get_fresh_request_object()
{
    // We don't need to bzero() request's underlying memory. It's okay to leak
    // uninitialized data from our stack/heap to the process at the other end of
    // sockfd. The other process already has control over the environment we're
    // running under. In other words, there's nothing we're "leaking" that the
    // other process couldn't already access (if it actually wanted to).
    auto request = request_ptr{pool_allocator<struct request>::allocate()};
#if BOOST_OS_LINUX
    request->id = gettid();
#elif BOOST_OS_BSD_FREE
    std::ignore = thr_self(&request->id);
#else
    request->id = reinterpret_cast<std::uintptr_t>(&cookie_for_thread_id);
#endif
    return request;
}

static inline lua_filter_ptr get_lua_filter_from_pool_or_create()
{
    return lua_filter_ptr{};
}

static inline void add_lua_filter_to_pool(lua_filter_ptr)
{}

static void receive_with_fds(reply_with_metadata& reply, thread_id id)
{
    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = &reply;
    iov.iov_len = sizeof(struct reply);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    reply.fds.fill(-1);
    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int) * reply.fds.size())];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    auto nread = TEMP_FAILURE_RETRY(recvmsg(sockfd, &msg, /*flags=*/0));
    if (nread == -1 || nread == 0) {
        reply.id = id;
        reply.action = reply::FORWARD_TO_REAL_LIBC;
        return;
    }

    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
            continue;

        char* in = (char*)CMSG_DATA(cmsg);
        auto nfds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
        for (std::size_t i = 0 ; i != nfds ; ++i) {
            int fd;
            std::memcpy(&fd, in, sizeof(int));
            in += sizeof(int);
            if (fd != -1) {
                [[maybe_unused]] bool inserted = reply.push_fd(fd);
                assert(inserted);
            }
        }
    }

    if (msg.msg_flags & MSG_CTRUNC) {
        for (int& fd : reply.fds) {
            if (fd != -1) {
                std::ignore = close(fd);
                fd = -1;
            }
        }

        reply.action = reply::FORWARD_TO_REAL_LIBC;
        return;
    }
}

static reply_with_metadata_ptr get_reply(thread_id id)
{
    std::unique_lock<std::mutex> lk{receive_queue_mtx};
    for (;;) {
        if (receive_queue.contains(id)) {
            auto reply = std::move(receive_queue[id]);
            receive_queue.erase(id);
            return reply;
        }

        if (!reading) {
            reading = true;
            lk.unlock();
            BOOST_SCOPE_EXIT_ALL(&) {
                lk.lock();
                reading = false;
                receive_queue_changed_cond.notify_all();
            };

            for (;;) {
                reply_with_metadata_ptr reply{
                    pool_allocator<reply_with_metadata>::allocate()};
                receive_with_fds(*reply, id);
                if (reply->id == id)
                    return reply;

                [[maybe_unused]]
                std::lock_guard<std::mutex> scoped_lk{receive_queue_mtx};
                receive_queue[reply->id] = std::move(reply);
                receive_queue_changed_cond.notify_all();
            }
        }

        receive_queue_changed_cond.wait(lk);
    }
}

extern "C" {

static int forward_open(
    int (*real_open)(const char*, int, ...), fds_type& fds, const char* path,
    int oflag, ...)
{
    fds.fill(-1);

    auto request = get_fresh_request_object();
    request->function = request::OPEN;

    auto pathlen = std::strlen(path);
    if (request->buffer.size() < pathlen + 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    std::memcpy(request->buffer.data(), path, pathlen + 1);

    request->intargs[0] = oflag;

    bool has_mode =
        ((oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
        ((oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
        false;

    if (has_mode) {
        std::va_list args;
        va_start(args, oflag);
        static_assert(sizeof(int) >= sizeof(mode_t));
        request->intargs[1] = va_arg(args, mode_t);
        va_end(args);
    }

    if (
        TEMP_FAILURE_RETRY(
            write(sockfd, request.get(), sizeof(struct request))) == -1
    ) {
        if (has_mode) {
            mode_t mode = request->intargs[1];
            return real_open(path, oflag, mode);
        } else {
            return real_open(path, oflag);
        }
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        if (has_mode) {
            mode_t mode = request->intargs[1];
            return real_open(path, oflag, mode);
        } else {
            return real_open(path, oflag);
        }
    default:
        __builtin_unreachable();
    }
}

static int my_open(
    int (*real_open)(const char*, int, ...), const char* path, int oflag, ...)
{
    bool has_mode =
        ((oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
        ((oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
        false;

    if (!lua_filter::filters.contains(request::OPEN)) {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        if (has_mode) {
            std::va_list args;
            va_start(args, oflag);
            mode_t mode = va_arg(args, mode_t);
            va_end(args);
            return forward_open(real_open, fds, path, oflag, mode);
        } else {
            return forward_open(real_open, fds, path, oflag);
        }
    }

    auto lua_filter = get_lua_filter_from_pool_or_create();
    BOOST_SCOPE_EXIT_ALL(&) { add_lua_filter_to_pool(std::move(lua_filter)); };
    auto L = lua_filter->L;
    lua_pushlightuserdata(L, &lua_filter::open_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(real_open));
    lua_pushcclosure(L, [](lua_State* L) -> int {
        auto real_open = reinterpret_cast<int (*)(const char*, int, ...)>(
            lua_touserdata(L, lua_upvalueindex(1)));
        const char* path = luaL_checkstring(L, 1);
        int oflag = luaL_checkinteger(L, 2);
        fds_type fds;
        int res;
        if (
            ((oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
            ((oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
            false
        ) {
            mode_t mode = luaL_checkinteger(L, 3);
            res = forward_open(real_open, fds, path, oflag, mode);
        } else {
            res = forward_open(real_open, fds, path, oflag);
        }
        int open_errno = (res == -1) ? errno : 0;

        int ret = 2;
        lua_pushinteger(L, res);
        lua_pushinteger(L, open_errno);

        for (int fd : fds) {
            if (fd == -1)
                break;

            lua_pushinteger(L, fd);
            ++ret;
        }

        return ret;
    }, 1);
    lua_pushstring(L, path);
    lua_pushinteger(L, oflag);
    mode_t mode;
    if (has_mode) {
        std::va_list args;
        va_start(args, oflag);
        mode = va_arg(args, mode_t);
        va_end(args);
        lua_pushinteger(L, mode);
    }

    auto on_lua_fail = [&]() -> int {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        if (has_mode) {
            return forward_open(real_open, fds, path, oflag, mode);
        } else {
            return forward_open(real_open, fds, path, oflag);
        }
    };

    if (lua_pcall(
        L, /*nargs=*/has_mode ? 4 : 3, /*nresults=*/2, /*errfunc=*/0) != 0
    ) {
        lua_pop(L, 1);
        return on_lua_fail();
    }

    if (lua_type(L, -2) != LUA_TNUMBER) {
        lua_pop(L, 2);
        return on_lua_fail();
    }
    int res = lua_tointeger(L, -2);
    switch (lua_type(L, -1)) {
    default:
        lua_pop(L, 2);
        return on_lua_fail();
    case LUA_TNIL:
        lua_pop(L, 2);
        break;
    case LUA_TNUMBER: {
        auto saved_errno = lua_tointeger(L, -1);
        lua_pop(L, 2);
        errno = saved_errno;
        break;
    }
    }
    return res;
}

static int forward_openat2(
    int (*real_openat2)(int, const char*, open_how*),
    fds_type& fds, int dirfd, const char* path, open_how* how)
{
    fds.fill(-1);

    if (fcntl(dirfd, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::OPENAT;

    auto pathlen = std::strlen(path);
    if (request->buffer.size() < pathlen + 1) {
        errno = ENAMETOOLONG;
        return -1;
    }
    std::memcpy(request->buffer.data(), path, pathlen + 1);

    request->intargs[0] = how->mode;
    request->uintargs[0] = how->flags;
    request->uintargs[1] = how->resolve;

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &dirfd, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        return real_openat2(dirfd, path, how);
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_openat2(dirfd, path, how);
    default:
        __builtin_unreachable();
    }
}

static int my_openat2(
    int (*real_openat2)(int, const char*, open_how*),
    int dirfd, const char* path, open_how* how)
{
    if (!lua_filter::filters.contains(request::OPENAT)) {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_openat2(real_openat2, fds, dirfd, path, how);
    }

    auto lua_filter = get_lua_filter_from_pool_or_create();
    BOOST_SCOPE_EXIT_ALL(&) { add_lua_filter_to_pool(std::move(lua_filter)); };
    auto L = lua_filter->L;
    lua_pushlightuserdata(L, &lua_filter::openat_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(real_openat2));
    lua_pushcclosure(L, [](lua_State* L) -> int {
        auto real_openat2 = reinterpret_cast<
            int (*)(int, const char*, open_how*)
        >(lua_touserdata(L, lua_upvalueindex(1)));
        int dirfd = luaL_checkinteger(L, 1);
        const char* path = luaL_checkstring(L, 2);
        open_how how;
        std::memset(&how, 0, sizeof(how));
        how.flags = luaL_checkinteger(L, 3);
        how.mode = luaL_checkinteger(L, 4);
        luaL_checktype(L, 5, LUA_TTABLE);

        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, 5, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for;
            case LUA_TSTRING:
                break;
            default:
                return luaL_error(L, "invalid argument for resolve");
            }

            std::string_view strv;
            {
                std::size_t len;
                const char* str = lua_tolstring(L, -1, &len);
                strv = std::string_view{str, len};
            }
            if (strv == "beneath") {
                how.resolve |= open_how::resolve_beneath;
            } else if (strv == "in_root") {
                how.resolve |= open_how::resolve_in_root;
            } else if (strv == "no_magiclinks") {
                how.resolve |= open_how::resolve_no_magiclinks;
            } else if (strv == "no_symlinks") {
                how.resolve |= open_how::resolve_no_symlinks;
            } else if (strv == "no_xdev") {
                how.resolve |= open_how::resolve_no_xdev;
            } else if (strv == "cached") {
                how.resolve |= open_how::resolve_cached;
            } else {
                return luaL_error(L, "invalid argument for resolve");
            }
            lua_pop(L, 1);
        }
        end_for:

        fds_type fds;
        int res = forward_openat2(real_openat2, fds, dirfd, path, &how);
        int openat2_errno = (res == -1) ? errno : 0;

        int ret = 2;
        lua_pushinteger(L, res);
        lua_pushinteger(L, openat2_errno);

        for (int fd : fds) {
            if (fd == -1)
                break;

            lua_pushinteger(L, fd);
            ++ret;
        }

        return ret;
    }, 1);
    lua_pushinteger(L, dirfd);
    lua_pushstring(L, path);
    lua_pushinteger(L, how->flags);
    lua_pushinteger(L, how->mode);
    lua_newtable(L);
    {
        auto resolve = how->resolve;
        int i = 1;

        if (
            (resolve & open_how::resolve_beneath) == open_how::resolve_beneath
        ) {
            resolve &= ~open_how::resolve_beneath;
            lua_pushliteral(L, "beneath");
            lua_rawseti(L, -2, i++);
        }

        if (
            (resolve & open_how::resolve_in_root) == open_how::resolve_in_root
        ) {
            resolve &= ~open_how::resolve_in_root;
            lua_pushliteral(L, "in_root");
            lua_rawseti(L, -2, i++);
        }

        if (
            (resolve & open_how::resolve_no_magiclinks) ==
            open_how::resolve_no_magiclinks
        ) {
            resolve &= ~open_how::resolve_no_magiclinks;
            lua_pushliteral(L, "no_magiclinks");
            lua_rawseti(L, -2, i++);
        }

        if (
            (resolve & open_how::resolve_no_symlinks) ==
            open_how::resolve_no_symlinks
        ) {
            resolve &= ~open_how::resolve_no_symlinks;
            lua_pushliteral(L, "no_symlinks");
            lua_rawseti(L, -2, i++);
        }

        if (
            (resolve & open_how::resolve_no_xdev) == open_how::resolve_no_xdev
        ) {
            resolve &= ~open_how::resolve_no_xdev;
            lua_pushliteral(L, "no_xdev");
            lua_rawseti(L, -2, i++);
        }

        if (
            (resolve & open_how::resolve_cached) == open_how::resolve_cached
        ) {
            resolve &= ~open_how::resolve_cached;
            lua_pushliteral(L, "cached");
            lua_rawseti(L, -2, i++);
        }

        // For now it'll never enter here. However once glibc adds its own
        // wrapper for openat2() -- and Emilua start interposing it -- flags
        // known to Emilua could become out-of-sync with calls to openat2() done
        // by 3rd party code. Better to add this unused error cheking now than
        // risking forgetting to add it when needed later.
        if (resolve != 0) {
            lua_settop(L, 0);
            errno = ENOTSUP;
            return -1;
        }
    }

    auto on_lua_fail = [&]() -> int {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_openat2(real_openat2, fds, dirfd, path, how);
    };

    if (lua_pcall(L, /*nargs=*/6, /*nresults=*/2, /*errfunc=*/0) != 0) {
        lua_pop(L, 1);
        return on_lua_fail();
    }

    if (lua_type(L, -2) != LUA_TNUMBER) {
        lua_pop(L, 2);
        return on_lua_fail();
    }
    int res = lua_tointeger(L, -2);
    switch (lua_type(L, -1)) {
    default:
        lua_pop(L, 2);
        return on_lua_fail();
    case LUA_TNIL:
        lua_pop(L, 2);
        break;
    case LUA_TNUMBER: {
        auto saved_errno = lua_tointeger(L, -1);
        lua_pop(L, 2);
        errno = saved_errno;
        break;
    }
    }
    return res;
}

static int forward_unlink(
    int (*real_unlink)(const char*), fds_type& fds, const char* path)
{
    fds.fill(-1);

    auto request = get_fresh_request_object();
    request->function = request::UNLINK;

    {
        std::span<char> buffer = request->buffer;

        std::string_view pathv{path};
        if (pathv.size() > buffer.size()) {
            errno = ENAMETOOLONG;
            return -1;
        }
        std::memcpy(buffer.data(), pathv.data(), pathv.size());
        buffer = buffer.last(buffer.size() - pathv.size());
        request->uintargs[0] = pathv.size();
    }

    if (
        TEMP_FAILURE_RETRY(
            write(sockfd, request.get(), sizeof(struct request))) == -1
    ) {
        return real_unlink(path);
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_unlink(path);
    default:
        __builtin_unreachable();
    }
}

static int my_unlink(int (*real_unlink)(const char*), const char* pathname)
{
    if (!lua_filter::filters.contains(request::UNLINK)) {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_unlink(real_unlink, fds, pathname);
    }

    auto lua_filter = get_lua_filter_from_pool_or_create();
    BOOST_SCOPE_EXIT_ALL(&) { add_lua_filter_to_pool(std::move(lua_filter)); };
    auto L = lua_filter->L;
    lua_pushlightuserdata(L, &lua_filter::unlink_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(real_unlink));
    lua_pushcclosure(L, [](lua_State* L) -> int {
        auto real_unlink = reinterpret_cast<int (*)(const char*)>(
            lua_touserdata(L, lua_upvalueindex(1)));
        const char* path = luaL_checkstring(L, 1);
        fds_type fds;
        int res = forward_unlink(real_unlink, fds, path);
        int unlink_errno = (res == -1) ? errno : 0;

        int ret = 2;
        lua_pushinteger(L, res);
        lua_pushinteger(L, unlink_errno);

        for (int fd : fds) {
            if (fd == -1)
                break;

            lua_pushinteger(L, fd);
            ++ret;
        }

        return ret;
    }, 1);
    lua_pushstring(L, pathname);

    auto on_lua_fail = [&]() -> int {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_unlink(real_unlink, fds, pathname);
    };

    if (lua_pcall(L, /*nargs=*/2, /*nresults=*/2, /*errfunc=*/0) != 0) {
        lua_pop(L, 1);
        return on_lua_fail();
    }

    if (lua_type(L, -2) != LUA_TNUMBER) {
        lua_pop(L, 2);
        return on_lua_fail();
    }
    int res = lua_tointeger(L, -2);
    switch (lua_type(L, -1)) {
    default:
        lua_pop(L, 2);
        return on_lua_fail();
    case LUA_TNIL:
        lua_pop(L, 2);
        break;
    case LUA_TNUMBER: {
        auto saved_errno = lua_tointeger(L, -1);
        lua_pop(L, 2);
        errno = saved_errno;
        break;
    }
    }
    return res;
}

static int forward_rename(
    int (*real_rename)(const char*, const char*),
    fds_type& fds, const char* path1, const char* path2)
{
    fds.fill(-1);

    auto request = get_fresh_request_object();
    request->function = request::RENAME;

    {
        std::span<char> buffer = request->buffer;

        std::string_view path1v{path1};
        if (path1v.size() > buffer.size()) {
            errno = ENAMETOOLONG;
            return -1;
        }
        std::memcpy(buffer.data(), path1v.data(), path1v.size());
        buffer = buffer.last(buffer.size() - path1v.size());
        request->uintargs[0] = path1v.size();

        std::string_view path2v{path2};
        if (path2v.size() > buffer.size()) {
            errno = ENAMETOOLONG;
            return -1;
        }
        std::memcpy(buffer.data(), path2v.data(), path2v.size());
        buffer = buffer.last(buffer.size() - path2v.size());
        request->uintargs[1] = path2v.size();
    }

    if (
        TEMP_FAILURE_RETRY(
            write(sockfd, request.get(), sizeof(struct request))) == -1
    ) {
        return real_rename(path1, path2);
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_rename(path1, path2);
    default:
        __builtin_unreachable();
    }
}

static int my_rename(
    int (*real_rename)(const char*, const char*),
    const char* path1, const char* path2)
{
    if (!lua_filter::filters.contains(request::RENAME)) {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_rename(real_rename, fds, path1, path2);
    }

    auto lua_filter = get_lua_filter_from_pool_or_create();
    BOOST_SCOPE_EXIT_ALL(&) { add_lua_filter_to_pool(std::move(lua_filter)); };
    auto L = lua_filter->L;
    lua_pushlightuserdata(L, &lua_filter::rename_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(real_rename));
    lua_pushcclosure(L, [](lua_State* L) -> int {
        auto real_rename = reinterpret_cast<int (*)(const char*, const char*)>(
            lua_touserdata(L, lua_upvalueindex(1)));
        const char* path1 = luaL_checkstring(L, 1);
        const char* path2 = luaL_checkstring(L, 2);
        fds_type fds;
        int res = forward_rename(real_rename, fds, path1, path2);
        int rename_errno = (res == -1) ? errno : 0;

        int ret = 2;
        lua_pushinteger(L, res);
        lua_pushinteger(L, rename_errno);

        for (int fd : fds) {
            if (fd == -1)
                break;

            lua_pushinteger(L, fd);
            ++ret;
        }

        return ret;
    }, 1);
    lua_pushstring(L, path1);
    lua_pushstring(L, path2);

    auto on_lua_fail = [&]() -> int {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_rename(real_rename, fds, path1, path2);
    };

    if (lua_pcall(L, /*nargs=*/3, /*nresults=*/2, /*errfunc=*/0) != 0) {
        lua_pop(L, 1);
        return on_lua_fail();
    }

    if (lua_type(L, -2) != LUA_TNUMBER) {
        lua_pop(L, 2);
        return on_lua_fail();
    }
    int res = lua_tointeger(L, -2);
    switch (lua_type(L, -1)) {
    default:
        lua_pop(L, 2);
        return on_lua_fail();
    case LUA_TNIL:
        lua_pop(L, 2);
        break;
    case LUA_TNUMBER: {
        auto saved_errno = lua_tointeger(L, -1);
        lua_pop(L, 2);
        errno = saved_errno;
        break;
    }
    }
    return res;
}

static int forward_connect_unix(
    int (*real_connect)(int, const struct sockaddr*, socklen_t),
    fds_type& fds, int s, std::string_view path)
{
    fds.fill(-1);

    if (fcntl(s, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::CONNECT_UNIX;

    if (
        path.size() > request->buffer.size() ||
        path.size() > sizeof(std::declval<struct sockaddr_un>().sun_path)
    ) {
        errno = ENAMETOOLONG;
        return -1;
    }
    std::memcpy(request->buffer.data(), path.data(), path.size());
    request->uintargs[0] = path.size();

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &s, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path.data(), path.size());

        socklen_t addrlen = offsetof(struct sockaddr_un, sun_path) +
            path.size();

        return real_connect(
            s, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC: {
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path.data(), path.size());

        socklen_t addrlen = offsetof(struct sockaddr_un, sun_path) +
            path.size();

        return real_connect(
            s, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    }
    default:
        __builtin_unreachable();
    }
}

static int forward_connect_inet(
    int (*real_connect)(int, const struct sockaddr*, socklen_t),
    fds_type& fds, int s, const struct sockaddr_in* addr)
{
    fds.fill(-1);

    if (fcntl(s, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::CONNECT_INET;

    std::memcpy(request->buffer.data(), addr, sizeof(struct sockaddr_in));

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &s, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        return real_connect(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in));
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_connect(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in));
    default:
        __builtin_unreachable();
    }
}

static int forward_connect_inet6(
    int (*real_connect)(int, const struct sockaddr*, socklen_t),
    fds_type& fds, int s, const struct sockaddr_in6* addr)
{
    fds.fill(-1);

    if (fcntl(s, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::CONNECT_INET6;

    std::memcpy(request->buffer.data(), addr, sizeof(struct sockaddr_in6));

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &s, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        return real_connect(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in6));
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_connect(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in6));
    default:
        __builtin_unreachable();
    }
}

static int my_connect(
    int (*real_connect)(int, const struct sockaddr*, socklen_t),
    int s, const struct sockaddr* name, socklen_t namelen)
{
    switch (name->sa_family) {
    default:
        return real_connect(s, name, namelen);
    case AF_UNIX: {
        if (namelen == sizeof(sa_family_t)) {
            // unnamed
            return real_connect(s, name, namelen);
        }

        auto usock = reinterpret_cast<const struct sockaddr_un*>(name);
        auto plen = namelen - offsetof(struct sockaddr_un, sun_path);
        if (usock->sun_path[0] != '\0' && usock->sun_path[plen - 1] != '\0') {
            // For maximum portability, socklen_t should also count the
            // terminating null byte. However there's real-world code omitting
            // the terminating null byte from the count given by socklen_t.
            ++plen;

            if (plen > sizeof(usock->sun_path)) {
                errno = ENAMETOOLONG;
                return -1;
            }

            if (usock->sun_path[plen - 1] != '\0') {
                // we can't modify a read-only argument, so we just bail out of
                // this slowly never-ending mess
                errno = EINVAL;
                return -1;
            }
        }
        std::string_view addr{usock->sun_path, plen};

        if (!lua_filter::filters.contains(request::CONNECT_UNIX)) {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_connect_unix(real_connect, fds, s, addr);
        }

        auto lua_filter = get_lua_filter_from_pool_or_create();
        BOOST_SCOPE_EXIT_ALL(&) {
            add_lua_filter_to_pool(std::move(lua_filter));
        };
        auto L = lua_filter->L;
        lua_pushlightuserdata(L, &lua_filter::connect_unix_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(real_connect));
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto real_connect = reinterpret_cast<
                int (*)(int, const struct sockaddr*, socklen_t)
            >(lua_touserdata(L, lua_upvalueindex(1)));
            int fd = luaL_checkinteger(L, 1);

            std::string_view addr;
            {
                std::size_t pathlen;
                const char* path = luaL_checklstring(L, 2, &pathlen);
                if (path[0] == '\0') {
                    addr = std::string_view{path, pathlen};
                } else {
                    // if address is a pathname (i.e. not an abstract socket
                    // address), include the null byte as well for the C layer
                    addr = std::string_view{path, pathlen + 1};
                }
            }

            fds_type fds;
            int res = forward_connect_unix(real_connect, fds, fd, addr);
            int connect_errno = (res == -1) ? errno : 0;

            int ret = 2;
            lua_pushinteger(L, res);
            lua_pushinteger(L, connect_errno);

            for (int fd : fds) {
                if (fd == -1)
                    break;

                lua_pushinteger(L, fd);
                ++ret;
            }

            return ret;
        }, 1);
        lua_pushinteger(L, s);
        if (addr[0] == '\0') {
            lua_pushlstring(L, addr.data(), addr.size());
        } else {
            // if address is a pathname (i.e. not an abstract socket address),
            // don't include the null byte for the Lua layer
            lua_pushlstring(L, addr.data(), addr.size() - 1);
        }

        auto on_lua_fail = [&]() -> int {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_connect_unix(real_connect, fds, s, addr);
        };

        if (lua_pcall(L, /*nargs=*/3, /*nresults=*/2, /*errfunc=*/0) != 0) {
            lua_pop(L, 1);
            return on_lua_fail();
        }

        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return on_lua_fail();
        }
        int res = lua_tointeger(L, -2);
        switch (lua_type(L, -1)) {
        default:
            lua_pop(L, 2);
            return on_lua_fail();
        case LUA_TNIL:
            lua_pop(L, 2);
            break;
        case LUA_TNUMBER: {
            auto saved_errno = lua_tointeger(L, -1);
            lua_pop(L, 2);
            errno = saved_errno;
            break;
        }
        }
        return res;
    }
    case AF_INET: {
        auto isock = reinterpret_cast<const struct sockaddr_in*>(name);

        if (!lua_filter::filters.contains(request::CONNECT_INET)) {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_connect_inet(real_connect, fds, s, isock);
        }

        auto lua_filter = get_lua_filter_from_pool_or_create();
        BOOST_SCOPE_EXIT_ALL(&) {
            add_lua_filter_to_pool(std::move(lua_filter));
        };
        auto L = lua_filter->L;
        lua_pushlightuserdata(L, &lua_filter::connect_inet_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(real_connect));
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto real_connect = reinterpret_cast<
                int (*)(int, const struct sockaddr*, socklen_t)
            >(lua_touserdata(L, lua_upvalueindex(1)));
            int fd = luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TTABLE);

            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;

            for (int i = 1 ; i <= 4 ; ++i) {
                lua_rawgeti(L, 2, i);
                std::uint8_t byte = luaL_checkinteger(L, -1);
                lua_pop(L, 1);
                addr.sin_addr.s_addr <<= 8;
                addr.sin_addr.s_addr |= byte;
            }
            boost::endian::native_to_big_inplace(addr.sin_addr.s_addr);

            addr.sin_port = luaL_checkinteger(L, 3);
            boost::endian::native_to_big_inplace(addr.sin_port);

            fds_type fds;
            int res = forward_connect_inet(real_connect, fds, fd, &addr);
            int connect_errno = (res == -1) ? errno : 0;

            int ret = 2;
            lua_pushinteger(L, res);
            lua_pushinteger(L, connect_errno);

            for (int fd : fds) {
                if (fd == -1)
                    break;

                lua_pushinteger(L, fd);
                ++ret;
            }

            return ret;
        }, 1);
        lua_pushinteger(L, s);

        {
            lua_createtable(L, /*narr=*/4, /*nrec=*/0);

            std::uint32_t iaddr = isock->sin_addr.s_addr;
            boost::endian::big_to_native_inplace(iaddr);

            lua_pushinteger(L, (iaddr >> 24) & 0xFF);
            lua_rawseti(L, -2, 1);

            lua_pushinteger(L, (iaddr >> 16) & 0xFF);
            lua_rawseti(L, -2, 2);

            lua_pushinteger(L, (iaddr >> 8) & 0xFF);
            lua_rawseti(L, -2, 3);

            lua_pushinteger(L, iaddr & 0xFF);
            lua_rawseti(L, -2, 4);
        }

        lua_pushinteger(L, boost::endian::big_to_native(isock->sin_port));

        auto on_lua_fail = [&]() -> int {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_connect_inet(real_connect, fds, s, isock);
        };

        if (lua_pcall(L, /*nargs=*/4, /*nresults=*/2, /*errfunc=*/0) != 0) {
            lua_pop(L, 1);
            return on_lua_fail();
        }

        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return on_lua_fail();
        }
        int res = lua_tointeger(L, -2);
        switch (lua_type(L, -1)) {
        default:
            lua_pop(L, 2);
            return on_lua_fail();
        case LUA_TNIL:
            lua_pop(L, 2);
            break;
        case LUA_TNUMBER: {
            auto saved_errno = lua_tointeger(L, -1);
            lua_pop(L, 2);
            errno = saved_errno;
            break;
        }
        }
        return res;
    }
    case AF_INET6: {
        auto isock = reinterpret_cast<const struct sockaddr_in6*>(name);

        if (!lua_filter::filters.contains(request::CONNECT_INET6)) {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_connect_inet6(real_connect, fds, s, isock);
        }

        auto lua_filter = get_lua_filter_from_pool_or_create();
        BOOST_SCOPE_EXIT_ALL(&) {
            add_lua_filter_to_pool(std::move(lua_filter));
        };
        auto L = lua_filter->L;
        lua_pushlightuserdata(L, &lua_filter::connect_inet6_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(real_connect));
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto real_connect = reinterpret_cast<
                int (*)(int, const struct sockaddr*, socklen_t)
            >(lua_touserdata(L, lua_upvalueindex(1)));
            int fd = luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TTABLE);

            struct sockaddr_in6 addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin6_family = AF_INET6;

            for (int i = 0 ; i != 16 ; ++i) {
                lua_rawgeti(L, 2, i + 1);
                std::uint8_t byte = luaL_checkinteger(L, -1);
                lua_pop(L, 1);
                addr.sin6_addr.s6_addr[i] = byte;
            }

            addr.sin6_port = luaL_checkinteger(L, 3);
            boost::endian::native_to_big_inplace(addr.sin6_port);

            addr.sin6_scope_id = luaL_checkinteger(L, 4);

            fds_type fds;
            int res = forward_connect_inet6(real_connect, fds, fd, &addr);
            int connect_errno = (res == -1) ? errno : 0;

            int ret = 2;
            lua_pushinteger(L, res);
            lua_pushinteger(L, connect_errno);

            for (int fd : fds) {
                if (fd == -1)
                    break;

                lua_pushinteger(L, fd);
                ++ret;
            }

            return ret;
        }, 1);
        lua_pushinteger(L, s);

        lua_createtable(L, /*narr=*/16, /*nrec=*/0);
        for (int i = 0 ; i != 16 ; ++i) {
            lua_pushinteger(L, isock->sin6_addr.s6_addr[i]);
            lua_rawseti(L, -2, i + 1);
        }

        lua_pushinteger(L, boost::endian::big_to_native(isock->sin6_port));

        lua_pushinteger(L, isock->sin6_scope_id);

        auto on_lua_fail = [&]() -> int {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_connect_inet6(real_connect, fds, s, isock);
        };

        if (lua_pcall(L, /*nargs=*/5, /*nresults=*/2, /*errfunc=*/0) != 0) {
            lua_pop(L, 1);
            return on_lua_fail();
        }

        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return on_lua_fail();
        }
        int res = lua_tointeger(L, -2);
        switch (lua_type(L, -1)) {
        default:
            lua_pop(L, 2);
            return on_lua_fail();
        case LUA_TNIL:
            lua_pop(L, 2);
            break;
        case LUA_TNUMBER: {
            auto saved_errno = lua_tointeger(L, -1);
            lua_pop(L, 2);
            errno = saved_errno;
            break;
        }
        }
        return res;
    }
    }
}

static int forward_bind_unix(
    int (*real_bind)(int, const struct sockaddr*, socklen_t),
    fds_type& fds, int s, std::string_view path)
{
    fds.fill(-1);

    if (fcntl(s, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::BIND_UNIX;

    if (
        path.size() > request->buffer.size() ||
        path.size() > sizeof(std::declval<struct sockaddr_un>().sun_path)
    ) {
        errno = ENAMETOOLONG;
        return -1;
    }
    std::memcpy(request->buffer.data(), path.data(), path.size());
    request->uintargs[0] = path.size();

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &s, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path.data(), path.size());

        socklen_t addrlen = offsetof(struct sockaddr_un, sun_path) +
            path.size();

        return real_bind(s, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC: {
        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path.data(), path.size());

        socklen_t addrlen = offsetof(struct sockaddr_un, sun_path) +
            path.size();

        return real_bind(s, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
    }
    default:
        __builtin_unreachable();
    }
}

static int forward_bind_inet(
    int (*real_bind)(int, const struct sockaddr*, socklen_t),
    fds_type& fds, int s, const struct sockaddr_in* addr)
{
    fds.fill(-1);

    if (fcntl(s, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::BIND_INET;

    std::memcpy(request->buffer.data(), addr, sizeof(struct sockaddr_in));

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &s, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        return real_bind(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in));
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_bind(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in));
    default:
        __builtin_unreachable();
    }
}

static int forward_bind_inet6(
    int (*real_bind)(int, const struct sockaddr*, socklen_t),
    fds_type& fds, int s, const struct sockaddr_in6* addr)
{
    fds.fill(-1);

    if (fcntl(s, F_GETFD) == -1 && errno == EBADF) {
        return -1;
    }

    auto request = get_fresh_request_object();
    request->function = request::BIND_INET6;

    std::memcpy(request->buffer.data(), addr, sizeof(struct sockaddr_in6));

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = request.get();
    iov.iov_len = sizeof(struct request);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &s, sizeof(int));

    if (TEMP_FAILURE_RETRY(sendmsg(sockfd, &msg, MSG_NOSIGNAL)) == -1) {
        return real_bind(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in6));
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        return reply->result;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_bind(
            s, reinterpret_cast<const struct sockaddr*>(addr),
            sizeof(struct sockaddr_in6));
    default:
        __builtin_unreachable();
    }
}

static int my_bind(
    int (*real_bind)(int, const struct sockaddr*, socklen_t),
    int s, const struct sockaddr* name, socklen_t namelen)
{
    switch (name->sa_family) {
    default:
        return real_bind(s, name, namelen);
    case AF_UNIX: {
        if (namelen == sizeof(sa_family_t)) {
            // unnamed
            return real_bind(s, name, namelen);
        }

        auto usock = reinterpret_cast<const struct sockaddr_un*>(name);
        auto plen = namelen - offsetof(struct sockaddr_un, sun_path);
        if (usock->sun_path[0] != '\0' && usock->sun_path[plen - 1] != '\0') {
            // For maximum portability, socklen_t should also count the
            // terminating null byte. However there's real-world code omitting
            // the terminating null byte from the count given by socklen_t.
            ++plen;

            if (plen > sizeof(usock->sun_path)) {
                errno = ENAMETOOLONG;
                return -1;
            }

            if (usock->sun_path[plen - 1] != '\0') {
                // we can't modify a read-only argument, so we just bail out of
                // this slowly never-ending mess
                errno = EINVAL;
                return -1;
            }
        }
        std::string_view addr{usock->sun_path, plen};

        if (!lua_filter::filters.contains(request::BIND_UNIX)) {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_bind_unix(real_bind, fds, s, addr);
        }

        auto lua_filter = get_lua_filter_from_pool_or_create();
        BOOST_SCOPE_EXIT_ALL(&) {
            add_lua_filter_to_pool(std::move(lua_filter));
        };
        auto L = lua_filter->L;
        lua_pushlightuserdata(L, &lua_filter::bind_unix_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(real_bind));
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto real_bind = reinterpret_cast<
                int (*)(int, const struct sockaddr*, socklen_t)
            >(lua_touserdata(L, lua_upvalueindex(1)));
            int fd = luaL_checkinteger(L, 1);

            std::string_view addr;
            {
                std::size_t pathlen;
                const char* path = luaL_checklstring(L, 2, &pathlen);
                if (path[0] == '\0') {
                    addr = std::string_view{path, pathlen};
                } else {
                    // if address is a pathname (i.e. not an abstract socket
                    // address), include the null byte as well for the C layer
                    addr = std::string_view{path, pathlen + 1};
                }
            }

            fds_type fds;
            int res = forward_bind_unix(real_bind, fds, fd, addr);
            int bind_errno = (res == -1) ? errno : 0;

            int ret = 2;
            lua_pushinteger(L, res);
            lua_pushinteger(L, bind_errno);

            for (int fd : fds) {
                if (fd == -1)
                    break;

                lua_pushinteger(L, fd);
                ++ret;
            }

            return ret;
        }, 1);
        lua_pushinteger(L, s);
        if (addr[0] == '\0') {
            lua_pushlstring(L, addr.data(), addr.size());
        } else {
            // if address is a pathname (i.e. not an abstract socket address),
            // don't include the null byte for the Lua layer
            lua_pushlstring(L, addr.data(), addr.size() - 1);
        }

        auto on_lua_fail = [&]() -> int {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_bind_unix(real_bind, fds, s, addr);
        };

        if (lua_pcall(L, /*nargs=*/3, /*nresults=*/2, /*errfunc=*/0) != 0) {
            lua_pop(L, 1);
            return on_lua_fail();
        }

        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return on_lua_fail();
        }
        int res = lua_tointeger(L, -2);
        switch (lua_type(L, -1)) {
        default:
            lua_pop(L, 2);
            return on_lua_fail();
        case LUA_TNIL:
            lua_pop(L, 2);
            break;
        case LUA_TNUMBER: {
            auto saved_errno = lua_tointeger(L, -1);
            lua_pop(L, 2);
            errno = saved_errno;
            break;
        }
        }
        return res;
    }
    case AF_INET: {
        auto isock = reinterpret_cast<const struct sockaddr_in*>(name);

        if (!lua_filter::filters.contains(request::BIND_INET)) {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_bind_inet(real_bind, fds, s, isock);
        }

        auto lua_filter = get_lua_filter_from_pool_or_create();
        BOOST_SCOPE_EXIT_ALL(&) {
            add_lua_filter_to_pool(std::move(lua_filter));
        };
        auto L = lua_filter->L;
        lua_pushlightuserdata(L, &lua_filter::bind_inet_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(real_bind));
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto real_bind = reinterpret_cast<
                int (*)(int, const struct sockaddr*, socklen_t)
            >(lua_touserdata(L, lua_upvalueindex(1)));
            int fd = luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TTABLE);

            struct sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;

            for (int i = 1 ; i <= 4 ; ++i) {
                lua_rawgeti(L, 2, i);
                std::uint8_t byte = luaL_checkinteger(L, -1);
                lua_pop(L, 1);
                addr.sin_addr.s_addr <<= 8;
                addr.sin_addr.s_addr |= byte;
            }
            boost::endian::native_to_big_inplace(addr.sin_addr.s_addr);

            addr.sin_port = luaL_checkinteger(L, 3);
            boost::endian::native_to_big_inplace(addr.sin_port);

            fds_type fds;
            int res = forward_bind_inet(real_bind, fds, fd, &addr);
            int bind_errno = (res == -1) ? errno : 0;

            int ret = 2;
            lua_pushinteger(L, res);
            lua_pushinteger(L, bind_errno);

            for (int fd : fds) {
                if (fd == -1)
                    break;

                lua_pushinteger(L, fd);
                ++ret;
            }

            return ret;
        }, 1);
        lua_pushinteger(L, s);

        {
            lua_createtable(L, /*narr=*/4, /*nrec=*/0);

            std::uint32_t iaddr = isock->sin_addr.s_addr;
            boost::endian::big_to_native_inplace(iaddr);

            lua_pushinteger(L, (iaddr >> 24) & 0xFF);
            lua_rawseti(L, -2, 1);

            lua_pushinteger(L, (iaddr >> 16) & 0xFF);
            lua_rawseti(L, -2, 2);

            lua_pushinteger(L, (iaddr >> 8) & 0xFF);
            lua_rawseti(L, -2, 3);

            lua_pushinteger(L, iaddr & 0xFF);
            lua_rawseti(L, -2, 4);
        }

        lua_pushinteger(L, boost::endian::big_to_native(isock->sin_port));

        auto on_lua_fail = [&]() -> int {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_bind_inet(real_bind, fds, s, isock);
        };

        if (lua_pcall(L, /*nargs=*/4, /*nresults=*/2, /*errfunc=*/0) != 0) {
            lua_pop(L, 1);
            return on_lua_fail();
        }

        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return on_lua_fail();
        }
        int res = lua_tointeger(L, -2);
        switch (lua_type(L, -1)) {
        default:
            lua_pop(L, 2);
            return on_lua_fail();
        case LUA_TNIL:
            lua_pop(L, 2);
            break;
        case LUA_TNUMBER: {
            auto saved_errno = lua_tointeger(L, -1);
            lua_pop(L, 2);
            errno = saved_errno;
            break;
        }
        }
        return res;
    }
    case AF_INET6: {
        auto isock = reinterpret_cast<const struct sockaddr_in6*>(name);

        if (!lua_filter::filters.contains(request::BIND_INET6)) {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_bind_inet6(real_bind, fds, s, isock);
        }

        auto lua_filter = get_lua_filter_from_pool_or_create();
        BOOST_SCOPE_EXIT_ALL(&) {
            add_lua_filter_to_pool(std::move(lua_filter));
        };
        auto L = lua_filter->L;
        lua_pushlightuserdata(L, &lua_filter::bind_inet6_key);
        lua_rawget(L, LUA_REGISTRYINDEX);
        lua_pushlightuserdata(L, reinterpret_cast<void*>(real_bind));
        lua_pushcclosure(L, [](lua_State* L) -> int {
            auto real_bind = reinterpret_cast<
                int (*)(int, const struct sockaddr*, socklen_t)
            >(lua_touserdata(L, lua_upvalueindex(1)));
            int fd = luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TTABLE);

            struct sockaddr_in6 addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin6_family = AF_INET6;

            for (int i = 0 ; i != 16 ; ++i) {
                lua_rawgeti(L, 2, i + 1);
                std::uint8_t byte = luaL_checkinteger(L, -1);
                lua_pop(L, 1);
                addr.sin6_addr.s6_addr[i] = byte;
            }

            addr.sin6_port = luaL_checkinteger(L, 3);
            boost::endian::native_to_big_inplace(addr.sin6_port);

            addr.sin6_scope_id = luaL_checkinteger(L, 4);

            fds_type fds;
            int res = forward_bind_inet6(real_bind, fds, fd, &addr);
            int bind_errno = (res == -1) ? errno : 0;

            int ret = 2;
            lua_pushinteger(L, res);
            lua_pushinteger(L, bind_errno);

            for (int fd : fds) {
                if (fd == -1)
                    break;

                lua_pushinteger(L, fd);
                ++ret;
            }

            return ret;
        }, 1);
        lua_pushinteger(L, s);

        lua_createtable(L, /*narr=*/16, /*nrec=*/0);
        for (int i = 0 ; i != 16 ; ++i) {
            lua_pushinteger(L, isock->sin6_addr.s6_addr[i]);
            lua_rawseti(L, -2, i + 1);
        }

        lua_pushinteger(L, boost::endian::big_to_native(isock->sin6_port));

        lua_pushinteger(L, isock->sin6_scope_id);

        auto on_lua_fail = [&]() -> int {
            fds_type fds;
            BOOST_SCOPE_EXIT_ALL(&) {
                for (int fd : fds) {
                    if (fd != -1)
                        std::ignore = close(fd);
                }
            };

            return forward_bind_inet6(real_bind, fds, s, isock);
        };

        if (lua_pcall(L, /*nargs=*/5, /*nresults=*/2, /*errfunc=*/0) != 0) {
            lua_pop(L, 1);
            return on_lua_fail();
        }

        if (lua_type(L, -2) != LUA_TNUMBER) {
            lua_pop(L, 2);
            return on_lua_fail();
        }
        int res = lua_tointeger(L, -2);
        switch (lua_type(L, -1)) {
        default:
            lua_pop(L, 2);
            return on_lua_fail();
        case LUA_TNIL:
            lua_pop(L, 2);
            break;
        case LUA_TNUMBER: {
            auto saved_errno = lua_tointeger(L, -1);
            lua_pop(L, 2);
            errno = saved_errno;
            break;
        }
        }
        return res;
    }
    }
}
static int forward_getaddrinfo(
    int (*real_getaddrinfo)(
        const char*, const char*, const struct addrinfo*, struct addrinfo**),
    fds_type& fds, const char* node, const char* service,
    const struct addrinfo* hints, struct addrinfo** res)
{
    fds.fill(-1);

    auto request = get_fresh_request_object();
    request->function = request::GETADDRINFO;

    if (!hints) {
        request->intargs[0] = 0;
    } else {
        switch (hints->ai_family) {
        default:
            request->intargs[0] = 0;
            break;
        case AF_UNSPEC:
        case AF_INET:
        case AF_INET6:
            switch (hints->ai_socktype) {
            default:
                request->intargs[0] = 0;
                break;
            case SOCK_STREAM:
                request->intargs[0] = IPPROTO_TCP;
                break;
            case SOCK_DGRAM:
                request->intargs[0] = IPPROTO_UDP;
                break;
            }
        }
    }

    {
        std::span<char> buffer = request->buffer;

        std::string_view nodev{node};
        if (nodev.size() > buffer.size()) {
            return EAI_MEMORY;
        }
        std::memcpy(buffer.data(), nodev.data(), nodev.size());
        buffer = buffer.last(buffer.size() - nodev.size());
        request->uintargs[0] = nodev.size();

        std::string_view servicev{service};
        if (servicev.size() > buffer.size()) {
            return EAI_MEMORY;
        }
        std::memcpy(buffer.data(), servicev.data(), servicev.size());
        buffer = buffer.last(buffer.size() - servicev.size());
        request->uintargs[1] = servicev.size();
    }

    if (
        TEMP_FAILURE_RETRY(
            write(sockfd, request.get(), sizeof(struct request))) == -1
    ) {
        return real_getaddrinfo(node, service, hints, res);
    }

    auto reply = get_reply(request->id);
    std::memcpy(fds.data(), reply->fds.data(), sizeof(int) * fds.size());
    switch (reply->action) {
    case reply::USE_REPLY_RESULT:
        errno = reply->error_code;
        switch (reply->result) {
        default:
            return reply->result;
        case 0: {
            // sin6_scope_id size (we don't accept interface names here so it's
            // safe to ignore IFNAMSIZ)
            static constexpr auto scope_id_strsz =
                std::numeric_limits<std::uint32_t>::digits10;

            char node2[
                // already takes the terminating null byte into account
                INET6_ADDRSTRLEN +

                // the % sign for the scope id delimiter
                1 +

                scope_id_strsz];

            switch (reply->intargs[0]) {
            case AF_UNSPEC: {
                struct addrinfo hints2;
                std::memset(&hints2, 0, sizeof(struct addrinfo));
                hints2.ai_family = AF_UNSPEC;
                hints2.ai_flags = AI_NUMERICSERV;

                // ".invalid" resolves to an empty list
                return real_getaddrinfo("a.invalid", "0", &hints2, res);
                break;
            }
            case AF_INET: {
                std::uint32_t s_addr;
                std::memcpy(&s_addr, reply->buffer.data(), sizeof(s_addr));
                if (!inet_ntop(AF_INET, &s_addr, node2, sizeof(node2))) {
                    return real_getaddrinfo(node, service, hints, res);
                }
                break;
            }
            case AF_INET6: {
                if (!inet_ntop(
                    AF_INET6, reply->buffer.data(), node2, sizeof(node2)
                )) {
                    return real_getaddrinfo(node, service, hints, res);
                }
                std::uint32_t scope_id = reply->intargs[2];
                if (scope_id != 0) {
                    auto iter = std::strchr(node2, '\0');
                    assert(*iter == '\0');
                    *iter++ = '%';
                    auto cvtres = std::to_chars(
                        iter, node2 + scope_id_strsz, scope_id);
                    assert(cvtres.ec == std::errc{});
                    *cvtres.ptr = '\0';
                }
                break;
            }
            default:
                assert(false);
            }

            char service2[std::numeric_limits<in_port_t>::digits10 + 1];
            {
                in_port_t port = reply->intargs[1];
                auto cvtres = std::to_chars(
                    service2, service2 + sizeof(service2) - 1, port);
                assert(cvtres.ec == std::errc{});
                *cvtres.ptr = '\0';
            }

            struct addrinfo hints2;
            std::memset(&hints2, 0, sizeof(struct addrinfo));
            hints2.ai_family = reply->intargs[0];
            hints2.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

            return real_getaddrinfo(node2, service2, &hints2, res);
        }
        }
        break;
    case reply::FORWARD_TO_REAL_LIBC:
        return real_getaddrinfo(node, service, hints, res);
    default:
        __builtin_unreachable();
    }
}

static int my_getaddrinfo(
    int (*real_getaddrinfo)(
        const char*, const char*, const struct addrinfo*, struct addrinfo**),
    const char* node, const char* service, const struct addrinfo* hints,
    struct addrinfo** res)
{
    if (!lua_filter::filters.contains(request::GETADDRINFO)) {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_getaddrinfo(
            real_getaddrinfo, fds, node, service, hints, res);
    }

    int protocol;
    if (!hints) {
        protocol = 0;
    } else {
        switch (hints->ai_family) {
        default:
            protocol = 0;
            break;
        case AF_UNSPEC:
        case AF_INET:
        case AF_INET6:
            switch (hints->ai_socktype) {
            default:
                protocol = 0;
                break;
            case SOCK_STREAM:
                protocol = IPPROTO_TCP;
                break;
            case SOCK_DGRAM:
                protocol = IPPROTO_UDP;
                break;
            }
        }
    }

    auto lua_filter = get_lua_filter_from_pool_or_create();
    BOOST_SCOPE_EXIT_ALL(&) { add_lua_filter_to_pool(std::move(lua_filter)); };
    auto L = lua_filter->L;
    lua_pushlightuserdata(L, &lua_filter::getaddrinfo_key);
    lua_rawget(L, LUA_REGISTRYINDEX);
    lua_pushlightuserdata(L, reinterpret_cast<void*>(real_getaddrinfo));
    lua_pushcclosure(L, [](lua_State* L) -> int {
        lua_settop(L, 3);

        auto real_getaddrinfo = reinterpret_cast<int (*)(
            const char*, const char*, const struct addrinfo*, struct addrinfo**
        )>(lua_touserdata(L, lua_upvalueindex(1)));
        const char* node = luaL_checkstring(L, 1);
        const char* service = luaL_checkstring(L, 2);
        int protocol;
        switch (lua_type(L, 3)) {
        default:
            return luaL_error(L, "invalid argument for protocol");
        case LUA_TNIL:
            protocol = 0;
            break;
        case LUA_TSTRING: {
            std::size_t len;
            const char* str = luaL_checklstring(L, 3, &len);
            std::string_view strv{str, len};

            if (strv == "tcp") {
                protocol = IPPROTO_TCP;
            } else if (strv == "udp") {
                protocol = IPPROTO_UDP;
            } else {
                return luaL_error(L, "invalid argument for protocol");
            }
            break;
        }
        }

        fds_type fds;

        struct addrinfo hints;
        std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        switch (protocol) {
        case 0:
            break;
        case IPPROTO_TCP:
            hints.ai_socktype = SOCK_STREAM;
            break;
        case IPPROTO_UDP:
            hints.ai_socktype = SOCK_DGRAM;
            break;
        default:
            assert(false);
        }
        hints.ai_protocol = protocol;

        struct addrinfo* res = NULL;
        int res2 = forward_getaddrinfo(
            real_getaddrinfo, fds, node, service, &hints, &res);
        int getaddrinfo_errno = (res2 == EAI_SYSTEM) ? errno : 0;

        if (res2 != 0) {
            switch (res2) {
            default:
                lua_pushinteger(L, res2);
                break;
            case EAI_AGAIN:
                lua_pushliteral(L, "again");
                break;
            case EAI_BADFLAGS:
                lua_pushliteral(L, "badflags");
                break;
            case EAI_FAIL:
                lua_pushliteral(L, "fail");
                break;
            case EAI_FAMILY:
                lua_pushliteral(L, "family");
                break;
            case EAI_MEMORY:
                lua_pushliteral(L, "memory");
                break;
            case EAI_NONAME:
                lua_pushliteral(L, "noname");
                break;
            case EAI_SERVICE:
                lua_pushliteral(L, "service");
                break;
            case EAI_SOCKTYPE:
                lua_pushliteral(L, "socktype");
                break;
            case EAI_SYSTEM:
                lua_pushliteral(L, "system");
                break;
            }
            lua_pushinteger(L, getaddrinfo_errno);
            return 2;
        }

        if (!res)
            return 0;

        BOOST_SCOPE_EXIT_ALL(&) { freeaddrinfo(res); };

        if (res->ai_family != AF_INET && res->ai_family != AF_INET6) {
            lua_pushliteral(L, "system");
            lua_pushinteger(L, ENOTSUP);
            return 2;
        }

        lua_pushnil(L);

        in_port_t port;
        if (res->ai_family == AF_INET) {
            lua_createtable(L, /*narr=*/4, /*nrec=*/0);
            auto isock = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
            port = isock->sin_port;

            char as_bytes[4];
            std::memcpy(as_bytes, &isock->sin_addr, 4);
            for (int i = 0 ; i != 4 ; ++i) {
                lua_pushinteger(L, as_bytes[i]);
                lua_rawseti(L, -2, i + 1);
            }
        } else if (res->ai_family == AF_INET6) {
            lua_createtable(L, /*narr=*/17, /*nrec=*/0);
            auto isock = reinterpret_cast<struct sockaddr_in6*>(res->ai_addr);
            port = isock->sin6_port;

            for (int i = 0 ; i != 16 ; ++i) {
                lua_pushinteger(L, isock->sin6_addr.s6_addr[i]);
                lua_rawseti(L, -2, i + 1);
            }

            lua_pushinteger(L, isock->sin6_scope_id);
            lua_rawseti(L, -2, 17);
        } else {
            lua_pushliteral(L, "system");
            lua_pushinteger(L, ENOTSUP);
            return 2;
        }

        boost::endian::big_to_native_inplace(port);
        lua_pushinteger(L, port);

        int ret = 3;

        for (int fd : fds) {
            if (fd == -1)
                break;

            lua_pushinteger(L, fd);
            ++ret;
        }

        return ret;
    }, 1);
    lua_pushstring(L, node);
    lua_pushstring(L, service);
    switch (protocol) {
    case 0:
        lua_pushnil(L);
        break;
    case IPPROTO_TCP:
        lua_pushliteral(L, "tcp");
        break;
    case IPPROTO_UDP:
        lua_pushliteral(L, "udp");
        break;
    default:
        assert(false);
    }

    auto on_lua_fail = [&]() -> int {
        fds_type fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (int fd : fds) {
                if (fd != -1)
                    std::ignore = close(fd);
            }
        };

        return forward_getaddrinfo(
            real_getaddrinfo, fds, node, service, hints, res);
    };

    if (lua_pcall(L, /*nargs=*/4, /*nresults=*/3, /*errfunc=*/0) != 0) {
        lua_pop(L, 1);
        return on_lua_fail();
    }
    BOOST_SCOPE_EXIT_ALL(&) { lua_pop(L, 3); };

    switch (lua_type(L, -3)) {
    default:
        return on_lua_fail();
    case LUA_TNIL: {
        if (lua_isnil(L, -2) && lua_isnil(L, -1)) {
            *res = NULL;
            return 0;
        }

        struct addrinfo hints2;
        std::memset(&hints2, 0, sizeof(struct addrinfo));
        hints2.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV;

        // sin6_scope_id size (we don't accept interface names here so it's safe
        // to ignore IFNAMSIZ)
        static constexpr auto scope_id_strsz =
            std::numeric_limits<std::uint32_t>::digits10;
        char node2[
            // already takes the terminating null byte into account
            INET6_ADDRSTRLEN +

            // the % sign for the scope id delimiter
            1 +

            scope_id_strsz];

        switch (lua_type(L, -2)) {
        default:
            return on_lua_fail();
        case LUA_TTABLE:
            break;
        }
        switch (lua_objlen(L, -2)) {
        default:
            return on_lua_fail();
        case 4: {
            hints2.ai_family = AF_INET;
            char* node2it = node2;

            for (int i = 1 ; i <= 4 ; ++i) {
                lua_rawgeti(L, -2, i);
                if (lua_type(L, -1) != LUA_TNUMBER) {
                    lua_pop(L, 1);
                    return on_lua_fail();
                }
                std::uint8_t byte = lua_tointeger(L, -1);
                lua_pop(L, 1);

                auto cvtres = std::to_chars(
                    node2it, node2 + sizeof(node2) - 1, byte);
                assert(cvtres.ec == std::errc{});
                node2it = cvtres.ptr;
                *node2it++ = '.';
            }
            *(node2it - 1) = '\0';
            break;
        }
        case 17: {
            hints2.ai_family = AF_INET6;
            struct in6_addr sin6_addr;

            for (int i = 1 ; i <= 16 ; ++i) {
                lua_rawgeti(L, -2, i);
                if (lua_type(L, -1) != LUA_TNUMBER) {
                    lua_pop(L, 1);
                    return on_lua_fail();
                }
                std::uint8_t byte = lua_tointeger(L, -1);
                lua_pop(L, 1);
                sin6_addr.s6_addr[i - 1] = byte;
            }
            std::ignore = inet_ntop(AF_INET6, &sin6_addr, node2, sizeof(node2));

            lua_rawgeti(L, -2, 17);
            if (lua_type(L, -1) != LUA_TNUMBER) {
                lua_pop(L, 1);
                return on_lua_fail();
            }
            std::uint32_t scope_id = lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (scope_id != 0) {
                auto iter = std::strchr(node2, '\0');
                assert(*iter == '\0');
                *iter++ = '%';
                auto cvtres = std::to_chars(
                    iter, node2 + scope_id_strsz, scope_id);
                assert(cvtres.ec == std::errc{});
                *cvtres.ptr = '\0';
            }
            break;
        }
        }

        char service2[std::numeric_limits<in_port_t>::digits10 + 1];

        switch (lua_type(L, -1)) {
        default:
            return on_lua_fail();
        case LUA_TNUMBER:
            in_port_t port = lua_tonumber(L, -1);
            auto cvtres = std::to_chars(
                service2, service2 + sizeof(service2) - 1, port);
            assert(cvtres.ec == std::errc{});
            *cvtres.ptr = '\0';
            break;
        }

        return real_getaddrinfo(node2, service2, &hints2, res);
    }
    case LUA_TNUMBER:
        return lua_tointeger(L, -3);
    case LUA_TSTRING: {
        std::string_view value;
        {
            std::size_t len;
            const char* str = lua_tolstring(L, -3, &len);
            value = std::string_view{str, len};
        }
        if (value == "again") {
            return EAI_AGAIN;
        } else if (value == "badflags") {
            return EAI_BADFLAGS;
        } else if (value == "fail") {
            return EAI_FAIL;
        } else if (value == "family") {
            return EAI_FAMILY;
        } else if (value == "memory") {
            return EAI_MEMORY;
        } else if (value == "noname") {
            return EAI_NONAME;
        } else if (value == "service") {
            return EAI_SERVICE;
        } else if (value == "socktype") {
            return EAI_SOCKTYPE;
        } else if (value == "system") {
            switch (lua_type(L, -2)) {
            default:
                return on_lua_fail();
            case LUA_TNUMBER:
                errno = lua_tointeger(L, -2);
                return EAI_SYSTEM;
            }
        } else {
            return on_lua_fail();
        }
        break;
    }
    }
}

} // extern "C"

void proc_set(int sockfd, std::map<int, std::string> lua_chunk_filters)
{
    assert(sockfd != -1);
    assert(emilua::libc_service::sockfd == -1);
    emilua::libc_service::sockfd = sockfd;
    lua_filter::filters = std::move(lua_chunk_filters);
    ambient_authority.open = my_open;
    ambient_authority.unlink = my_unlink;
    ambient_authority.rename = my_rename;
    ambient_authority.connect = my_connect;
    ambient_authority.bind = my_bind;
    ambient_authority.getaddrinfo = my_getaddrinfo;
    ambient_authority.openat2 = my_openat2;
}

} // namespace emilua::libc_service

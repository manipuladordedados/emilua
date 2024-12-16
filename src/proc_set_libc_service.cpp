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
#include <sys/un.h>
#include <cstdarg>
#include <cassert>
#include <fcntl.h>
#include <memory>
#include <mutex>

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
    }

    ~lua_filter()
    {
        lua_close(L);
    }

    lua_State* L;

    static std::map<int, std::string> filters;
    static char open_key;
    static char connect_unix_key;
    static char connect_inet_key;
    static char connect_inet6_key;
};

std::map<int, std::string> lua_filter::filters;
char lua_filter::open_key;
char lua_filter::connect_unix_key;
char lua_filter::connect_inet_key;
char lua_filter::connect_inet6_key;

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

} // extern "C"

void proc_set(int sockfd, std::map<int, std::string> lua_chunk_filters)
{
    assert(sockfd != -1);
    assert(emilua::libc_service::sockfd == -1);
    emilua::libc_service::sockfd = sockfd;
    lua_filter::filters = std::move(lua_chunk_filters);
    ambient_authority.open = my_open;
    ambient_authority.connect = my_connect;
}

} // namespace emilua::libc_service

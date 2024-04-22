/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <boost/asio/random_access_file.hpp>
#include <boost/asio/stream_file.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/async_base.hpp>
#include <emilua/filesystem.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/unix.hpp>

#if BOOST_OS_UNIX
#include <sys/file.h>
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

extern unsigned char write_all_at_bytecode[];
extern std::size_t write_all_at_bytecode_size;
extern unsigned char write_at_least_at_bytecode[];
extern std::size_t write_at_least_at_bytecode_size;
extern unsigned char read_all_at_bytecode[];
extern std::size_t read_all_at_bytecode_size;
extern unsigned char read_at_least_at_bytecode[];
extern std::size_t read_at_least_at_bytecode_size;

EMILUA_GPERF_DECLS_BEGIN(file)
EMILUA_GPERF_NAMESPACE(emilua)
char file_key;
char file_stream_mt_key;
char file_random_access_mt_key;

static char stream_read_some_key;
static char stream_write_some_key;
static char random_access_read_some_at_key;
static char random_access_write_some_at_key;

#if BOOST_OS_UNIX
static char stream_lock_key;
static char stream_lock_shared_key;
static char random_access_lock_key;
static char random_access_lock_shared_key;

struct file_descriptor_box
{
    file_descriptor_box()
        : value{-1}
    {}

    file_descriptor_box(int value)
        : value{value}
    {}

    file_descriptor_box(const file_descriptor_box&) = delete;
    file_descriptor_box& operator=(const file_descriptor_box&) = delete;

    ~file_descriptor_box()
    {
        if (value == -1)
            return;

        close(value);
    }

    int value;
};

struct flock_operation : public pending_operation
{
    flock_operation() : pending_operation{/*shared_ownership=*/false} {}

    ~flock_operation()
    {
        if (thread.joinable()) thread.join();
    }

    void cancel() noexcept override
    {
#if BOOST_OS_LINUX
        if (!thread.joinable())
            return;

        if (EMILUA_CONFIG_EINTR_RTSIGNO != 0) {
            sigval val;
            val.sival_int = 2;
            for (;;) {
                int error = pthread_sigqueue(
                    thread.native_handle(), EMILUA_CONFIG_EINTR_RTSIGNO, val);
                if (error != EAGAIN)
                    break;
                std::this_thread::yield();
            }
        }
#endif // BOOST_OS_LINUX
    }

    std::thread thread;
};
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(file)

int byte_span_non_member_append(lua_State* L);

EMILUA_GPERF_DECLS_BEGIN(stream)
EMILUA_GPERF_NAMESPACE(emilua)
static int stream_open(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string path;
    try {
        auto p = static_cast<std::filesystem::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path = p->string();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    auto flags = static_cast<asio::file_base::flags>(lua_tointeger(L, 3));

    boost::system::error_code ec;
    file->open(path, flags, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_close(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_cancel(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_assign(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 2));
    if (!handle || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 2);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int stream_release(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file_descriptor_handle rawfd = file->release(ec);
#if BOOST_OS_WINDOWS
    SetHandleInformation(rawfd, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
#endif // BOOST_OS_WINDOWS
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
#if BOOST_OS_WINDOWS
            BOOL res = CloseHandle(rawfd);
#else // BOOST_OS_WINDOWS
            int res = close(rawfd);
#endif // BOOST_OS_WINDOWS
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}

static int stream_resize(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->resize(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int stream_seek(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);
    luaL_checktype(L, 3, LUA_TSTRING);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    asio::file_base::seek_basis whence;
    auto whence_str = tostringview(L, 3);
    if (whence_str == "set") {
        whence = asio::file_base::seek_set;
    } else if (whence_str == "cur") {
        whence = asio::file_base::seek_cur;
    } else if (whence_str == "end") {
        whence = asio::file_base::seek_end;
    } else {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    boost::system::error_code ec;
    auto ret = file->seek(lua_tointeger(L, 2), whence, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushnumber(L, ret);
    return 1;
}

#if BOOST_OS_UNIX
static int stream_basic_try_lock(lua_State* L, int operation)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    if (flock(file->native_handle(), operation | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            lua_pushboolean(L, 0);
            return 1;
        }

        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int stream_try_lock(lua_State* L)
{
    return stream_basic_try_lock(L, LOCK_EX);
}

static int stream_try_lock_shared(lua_State* L)
{
    return stream_basic_try_lock(L, LOCK_SH);
}

static int stream_unlock(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    if (flock(file->native_handle(), LOCK_UN) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(stream)

#if BOOST_OS_UNIX
static int stream_basic_lock(lua_State* L, int operation)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    auto handle = new flock_operation;
    vm_ctx->pending_operations.push_back(*handle);

#if BOOST_OS_LINUX
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto handle = static_cast<flock_operation*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            if (EMILUA_CONFIG_EINTR_RTSIGNO != 0) {
                sigval val;
                val.sival_int = 1;
                for (;;) {
                    int error = pthread_sigqueue(
                        handle->thread.native_handle(),
                        EMILUA_CONFIG_EINTR_RTSIGNO, val);
                    if (error != EAGAIN)
                        break;
                    std::this_thread::yield();
                }
            }
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);
#endif // BOOST_OS_LINUX

    boost::system::error_code ignored_ec;
    auto fdbox = std::make_shared<file_descriptor_box>(
        file->release(ignored_ec));
    assert(!ignored_ec);

    handle->thread = std::thread{[
        wg=vm_ctx->work_guard(),vm_ctx,current_fiber,handle,file,fdbox,operation
    ]() {
        // CAUTION: You need to mark local variables here (if any are added)
        // whose value changes across longjmp() as volatile (ISO/IEC 9899:2011
        // §7.13.2.1 ¶3). The lambda capture is not a local variable to the body
        // of the lambda expression, so it doesn't need the volatile qualifier.
        sigjmp_buf branch_to_errhandler;

        // the value of these variables don't need to be preserved across
        // longjmp(), so they aren't marked as volatile (nor initialized)
        int res, last_error;

        switch (sigsetjmp(branch_to_errhandler, /*savesigs=*/0)) {
        case 0: {
            // Nor const_cast<sigjmp_buf*volatile&>(longjmp_on_rtsigno_env) nor
            // atomic_signal_fence() are needed given:
            //
            // - A call to sigaction synchronizes-with any resulting invocation
            //   of the signal handler (longjmp_on_rtsigno). longjmp_on_rtsigno
            //   reads longjmp_on_rtsigno_env, so writes to it can't disappear
            //   on compiler optimizations (although they still can be torn,
            //   reordered, merged, etc). IOW, our source code is enough to
            //   prove to the compiler that writes to longjmp_on_rtsigno_env are
            //   visible/observable elsewhere (all bets were off by the time the
            //   function accessing the variable was taken).
            // - Async sighandlers (and the eventual indirect call to
            //   longjmp_on_rtsigno) only execute after the signal is unblocked
            //   (pthread_sigmask).
            // - Syscalls (pthread_sigmask included) are compiler barriers in
            //   any non-buggy libc.
            //
            // Also longjmp_on_rtsigno_env doesn't need to be atomic
            // (i.e. tearing is fine here).
            longjmp_on_rtsigno_env = &branch_to_errhandler;

            sigset_t set;
            int fd = fdbox->value;

            // ONLY ASYNC-SIGNAL-SAFE CODE ALLOWED HERE {{{
            if (EMILUA_CONFIG_EINTR_RTSIGNO != 0) {
                sigemptyset(&set);
                sigaddset(&set, EMILUA_CONFIG_EINTR_RTSIGNO);
                pthread_sigmask(SIG_UNBLOCK, &set, /*oldset=*/NULL);
            }

            res = flock(fd, operation);
            last_error = errno;

            if (EMILUA_CONFIG_EINTR_RTSIGNO != 0)
                pthread_sigmask(SIG_BLOCK, &set, /*oldset=*/NULL);
            // }}}

            // reuse the code below to post work to ioctx
            [[fallthrough]];
        }
            // We only force skipping through flock() a second time (even though
            // flock is idempotent) here because a file descriptor may be shared
            // among processes and a different process could have triggered
            // op=LOCK_UN. If that has happened, then the second flock() would
            // return EWOULDBLOCK which triggers ec=errc::interrupted below. We
            // don't want that because the fiber hasn't been interrupted if
            // we're here.
            if (false) {
        case 1:
                // It's possible that longjmp() occurred after the previous
                // flock() had already finished (then the possible success of
                // the operation must be communicated). However flock() is
                // idempotent, so it's safe to call it multiple times.
                res = flock(fdbox->value, operation | LOCK_NB);
                last_error = errno;
            }

        {
            std::error_code ec;
            if (res == -1) {
                if (last_error == EWOULDBLOCK) {
                    ec = errc::interrupted;
                } else {
                    ec = std::error_code{last_error, std::system_category()};
                }
            }

            vm_ctx->strand().post(
                [vm_ctx,current_fiber,handle,file,fdbox,ec]() mutable {
                    if (!vm_ctx->valid())
                        return;

                    vm_ctx->pending_operations.erase_and_dispose(
                        vm_ctx->pending_operations.iterator_to(*handle),
                        [](pending_operation* op) { delete op; });

                    if (file->native_handle() != INVALID_FILE_DESCRIPTOR) {
                        ec = make_error_code(std::errc::bad_file_descriptor);
                    } else {
                        boost::system::error_code ignored_ec;
                        file->assign(fdbox->value, ignored_ec);
                        assert(!ignored_ec);
                        fdbox->value = -1;
                    }

                    auto opt_args = vm_context::options::arguments;
                    vm_ctx->fiber_resume(
                        current_fiber,
                        hana::make_set(
                            hana::make_pair(opt_args, hana::make_tuple(ec))));
                },
                std::allocator<void>{}
            );
            break;
        }
        case 2:
            // VM died (notification arrived through flock_operation::cancel()).
            // Just exit to discard handles (fdbox dtor will close the fd).
            return;
        default:
            __builtin_unreachable();
        }
    }};

    return lua_yield(L, 0);
}

static int stream_lock(lua_State* L)
{
    return stream_basic_lock(L, LOCK_EX);
}

static int stream_lock_shared(lua_State* L)
{
    return stream_basic_lock(L, LOCK_SH);
}
#endif // BOOST_OS_UNIX

static int stream_read_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int stream_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(stream)
EMILUA_GPERF_NAMESPACE(emilua)
inline int stream_is_open(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    lua_pushboolean(L, file->is_open());
    return 1;
}

inline int stream_size(lua_State* L)
{
    auto file = static_cast<asio::stream_file*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ret = file->size(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushnumber(L, ret);
    return 1;
}
EMILUA_GPERF_DECLS_END(stream)

static int stream_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "open",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_open);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_assign);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_release);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "resize",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_resize);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "seek",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, stream_seek);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lock",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                rawgetp(L, LUA_REGISTRYINDEX, &stream_lock_key);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lock_shared",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                rawgetp(L, LUA_REGISTRYINDEX, &stream_lock_shared_key);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "try_lock",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, stream_try_lock);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "try_lock_shared",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, stream_try_lock_shared);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "unlock",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, stream_unlock);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &stream_read_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &stream_write_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", stream_is_open)
        EMILUA_GPERF_PAIR("size", stream_size)
    EMILUA_GPERF_END(key)(L);
}

static int stream_new(lua_State* L)
{
    int nargs = lua_gettop(L);
    auto& vm_ctx = get_vm_context(L);

    if (nargs == 0) {
        auto file = static_cast<asio::stream_file*>(
            lua_newuserdata(L, sizeof(asio::stream_file))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
        setmetatable(L, -2);
        new (file) asio::stream_file{vm_ctx.strand().context()};
        return 1;
    }

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

    auto file = static_cast<asio::stream_file*>(
        lua_newuserdata(L, sizeof(asio::stream_file))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_stream_mt_key);
    setmetatable(L, -2);
    new (file) asio::stream_file{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(random_access)
EMILUA_GPERF_NAMESPACE(emilua)
static int random_access_open(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string path;
    try {
        auto p = static_cast<std::filesystem::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path = p->string();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    auto flags = static_cast<asio::file_base::flags>(lua_tointeger(L, 3));

    boost::system::error_code ec;
    file->open(path, flags, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int random_access_close(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int random_access_cancel(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int random_access_assign(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 2));
    if (!handle || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 2);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int random_access_release(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    boost::system::error_code ec;
    int rawfd = file->release(ec);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
            int res = close(rawfd);
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}
#endif // BOOST_OS_UNIX

static int random_access_resize(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    file->resize(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int random_access_basic_try_lock(lua_State* L, int operation)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    if (flock(file->native_handle(), operation | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            lua_pushboolean(L, 0);
            return 1;
        }

        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int random_access_try_lock(lua_State* L)
{
    return random_access_basic_try_lock(L, LOCK_EX);
}

static int random_access_try_lock_shared(lua_State* L)
{
    return random_access_basic_try_lock(L, LOCK_SH);
}

static int random_access_unlock(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    if (flock(file->native_handle(), LOCK_UN) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(random_access)

#if BOOST_OS_UNIX
static int random_access_basic_lock(lua_State* L, int operation)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (file->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    auto handle = new flock_operation;
    vm_ctx->pending_operations.push_back(*handle);

#if BOOST_OS_LINUX
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto handle = static_cast<flock_operation*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            if (EMILUA_CONFIG_EINTR_RTSIGNO != 0) {
                sigval val;
                val.sival_int = 1;
                for (;;) {
                    int error = pthread_sigqueue(
                        handle->thread.native_handle(),
                        EMILUA_CONFIG_EINTR_RTSIGNO, val);
                    if (error != EAGAIN)
                        break;
                    std::this_thread::yield();
                }
            }
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);
#endif // BOOST_OS_LINUX

    boost::system::error_code ignored_ec;
    auto fdbox = std::make_shared<file_descriptor_box>(
        file->release(ignored_ec));
    assert(!ignored_ec);

    handle->thread = std::thread{[
        wg=vm_ctx->work_guard(),vm_ctx,current_fiber,handle,file,fdbox,operation
    ]() {
        // CAUTION: You need to mark local variables here (if any are added)
        // whose value changes across longjmp() as volatile (ISO/IEC 9899:2011
        // §7.13.2.1 ¶3). The lambda capture is not a local variable to the body
        // of the lambda expression, so it doesn't need the volatile qualifier.
        sigjmp_buf branch_to_errhandler;

        // the value of these variables don't need to be preserved across
        // longjmp(), so they aren't marked as volatile (nor initialized)
        int res, last_error;

        switch (sigsetjmp(branch_to_errhandler, /*savesigs=*/0)) {
        case 0: {
            // Nor const_cast<sigjmp_buf*volatile&>(longjmp_on_rtsigno_env) nor
            // atomic_signal_fence() are needed given:
            //
            // - A call to sigaction synchronizes-with any resulting invocation
            //   of the signal handler (longjmp_on_rtsigno). longjmp_on_rtsigno
            //   reads longjmp_on_rtsigno_env, so writes to it can't disappear
            //   on compiler optimizations (although they still can be torn,
            //   reordered, merged, etc). IOW, our source code is enough to
            //   prove to the compiler that writes to longjmp_on_rtsigno_env are
            //   visible/observable elsewhere (all bets were off by the time the
            //   function accessing the variable was taken).
            // - Async sighandlers (and the eventual indirect call to
            //   longjmp_on_rtsigno) only execute after the signal is unblocked
            //   (pthread_sigmask).
            // - Syscalls (pthread_sigmask included) are compiler barriers in
            //   any non-buggy libc.
            //
            // Also longjmp_on_rtsigno_env doesn't need to be atomic
            // (i.e. tearing is fine here).
            longjmp_on_rtsigno_env = &branch_to_errhandler;

            sigset_t set;
            int fd = fdbox->value;

            // ONLY ASYNC-SIGNAL-SAFE CODE ALLOWED HERE {{{
            if (EMILUA_CONFIG_EINTR_RTSIGNO != 0) {
                sigemptyset(&set);
                sigaddset(&set, EMILUA_CONFIG_EINTR_RTSIGNO);
                pthread_sigmask(SIG_UNBLOCK, &set, /*oldset=*/NULL);
            }

            res = flock(fd, operation);
            last_error = errno;

            if (EMILUA_CONFIG_EINTR_RTSIGNO != 0)
                pthread_sigmask(SIG_BLOCK, &set, /*oldset=*/NULL);
            // }}}

            // reuse the code below to post work to ioctx
            [[fallthrough]];
        }
            // We only force skipping through flock() a second time (even though
            // flock is idempotent) here because a file descriptor may be shared
            // among processes and a different process could have triggered
            // op=LOCK_UN. If that has happened, then the second flock() would
            // return EWOULDBLOCK which triggers ec=errc::interrupted below. We
            // don't want that because the fiber hasn't been interrupted if
            // we're here.
            if (false) {
        case 1:
                // It's possible that longjmp() occurred after the previous
                // flock() had already finished (then the possible success of
                // the operation must be communicated). However flock() is
                // idempotent, so it's safe to call it multiple times.
                res = flock(fdbox->value, operation | LOCK_NB);
                last_error = errno;
            }

        {
            std::error_code ec;
            if (res == -1) {
                if (last_error == EWOULDBLOCK) {
                    ec = errc::interrupted;
                } else {
                    ec = std::error_code{last_error, std::system_category()};
                }
            }

            vm_ctx->strand().post(
                [vm_ctx,current_fiber,handle,file,fdbox,ec]() mutable {
                    if (!vm_ctx->valid())
                        return;

                    vm_ctx->pending_operations.erase_and_dispose(
                        vm_ctx->pending_operations.iterator_to(*handle),
                        [](pending_operation* op) { delete op; });

                    if (file->native_handle() != INVALID_FILE_DESCRIPTOR) {
                        ec = make_error_code(std::errc::bad_file_descriptor);
                    } else {
                        boost::system::error_code ignored_ec;
                        file->assign(fdbox->value, ignored_ec);
                        assert(!ignored_ec);
                        fdbox->value = -1;
                    }

                    auto opt_args = vm_context::options::arguments;
                    vm_ctx->fiber_resume(
                        current_fiber,
                        hana::make_set(
                            hana::make_pair(opt_args, hana::make_tuple(ec))));
                },
                std::allocator<void>{}
            );
            break;
        }
        case 2:
            // VM died (notification arrived through flock_operation::cancel()).
            // Just exit to discard handles (fdbox dtor will close the fd).
            return;
        default:
            __builtin_unreachable();
        }
    }};

    return lua_yield(L, 0);
}

static int random_access_lock(lua_State* L)
{
    return random_access_basic_lock(L, LOCK_EX);
}

static int random_access_lock_shared(lua_State* L)
{
    return random_access_basic_lock(L, LOCK_SH);
}
#endif // BOOST_OS_UNIX

static int random_access_read_some_at(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 3));
    if (!bs || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_read_some_at(
        lua_tointeger(L, 2),
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int random_access_write_some_at(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    if (!file || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 3));
    if (!bs || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    file->async_write_some_at(
        lua_tointeger(L, 2),
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(random_access)
EMILUA_GPERF_NAMESPACE(emilua)
inline int random_access_is_open(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    lua_pushboolean(L, file->is_open());
    return 1;
}

inline int random_access_size(lua_State* L)
{
    auto file = static_cast<asio::random_access_file*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ret = file->size(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushnumber(L, ret);
    return 1;
}
EMILUA_GPERF_DECLS_END(random_access)

static int random_access_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "open",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, random_access_open);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, random_access_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, random_access_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, random_access_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, random_access_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "resize",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, random_access_resize);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lock",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                rawgetp(L, LUA_REGISTRYINDEX, &random_access_lock_key);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lock_shared",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                rawgetp(L, LUA_REGISTRYINDEX, &random_access_lock_shared_key);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "try_lock",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, random_access_try_lock);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "try_lock_shared",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, random_access_try_lock_shared);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "unlock",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, random_access_unlock);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read_some_at",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &random_access_read_some_at_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write_some_at",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &random_access_write_some_at_key);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", random_access_is_open)
        EMILUA_GPERF_PAIR("size", random_access_size)
    EMILUA_GPERF_END(key)(L);
}

static int random_access_new(lua_State* L)
{
    int nargs = lua_gettop(L);
    auto& vm_ctx = get_vm_context(L);

    if (nargs == 0) {
        auto file = static_cast<asio::random_access_file*>(
            lua_newuserdata(L, sizeof(asio::random_access_file))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
        setmetatable(L, -2);
        new (file) asio::random_access_file{vm_ctx.strand().context()};
        return 1;
    }

#if BOOST_OS_UNIX
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

    auto file = static_cast<asio::random_access_file*>(
        lua_newuserdata(L, sizeof(asio::random_access_file))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    setmetatable(L, -2);
    new (file) asio::random_access_file{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    file->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
#else // BOOST_OS_UNIX
    push(L, std::errc::invalid_argument, "arg", 1);
    return lua_error(L);
#endif // BOOST_OS_UNIX
}

void init_file(lua_State* L)
{
    lua_pushlightuserdata(L, &file_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/5);

        lua_pushliteral(L, "open_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/8);

            lua_pushliteral(L, "append");
            lua_pushinteger(L, asio::file_base::append);
            lua_rawset(L, -3);

            lua_pushliteral(L, "create");
            lua_pushinteger(L, asio::file_base::create);
            lua_rawset(L, -3);

            lua_pushliteral(L, "exclusive");
            lua_pushinteger(L, asio::file_base::exclusive);
            lua_rawset(L, -3);

            lua_pushliteral(L, "read_only");
            lua_pushinteger(L, asio::file_base::read_only);
            lua_rawset(L, -3);

            lua_pushliteral(L, "read_write");
            lua_pushinteger(L, asio::file_base::read_write);
            lua_rawset(L, -3);

            lua_pushliteral(L, "sync_all_on_write");
            lua_pushinteger(L, asio::file_base::sync_all_on_write);
            lua_rawset(L, -3);

            lua_pushliteral(L, "truncate");
            lua_pushinteger(L, asio::file_base::truncate);
            lua_rawset(L, -3);

            lua_pushliteral(L, "write_only");
            lua_pushinteger(L, asio::file_base::write_only);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "write_all_at");
        {
            int res = luaL_loadbuffer(
                L, reinterpret_cast<char*>(write_all_at_bytecode),
                write_all_at_bytecode_size, nullptr);
            assert(res == 0); boost::ignore_unused(res);
            rawgetp(L, LUA_REGISTRYINDEX, &raw_type_key);
            lua_pushcfunction(L, byte_span_non_member_append);
            lua_call(L, 2, 1);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "write_at_least_at");
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(write_at_least_at_bytecode),
            write_at_least_at_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_all_at");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(read_all_at_bytecode),
            read_all_at_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "read_at_least_at");
        res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(read_at_least_at_bytecode),
            read_at_least_at_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        lua_rawset(L, -3);

        lua_pushliteral(L, "stream");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, stream_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "random_access");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, random_access_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_stream_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file.stream");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, stream_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::stream_file>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_random_access_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "file.random_access");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, random_access_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::random_access_file>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

#if BOOST_OS_UNIX
    lua_pushlightuserdata(L, &stream_lock_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, stream_lock);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &stream_lock_shared_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, stream_lock_shared);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &random_access_lock_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, random_access_lock);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &random_access_lock_shared_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, random_access_lock_shared);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // BOOST_OS_UNIX

    lua_pushlightuserdata(L, &stream_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, stream_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &stream_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, stream_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &random_access_read_some_at_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, random_access_read_some_at);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &random_access_write_some_at_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, random_access_write_some_at);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

/* Copyright (c) 2021, 2022, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/system.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>

#include <csignal>
#include <cstdlib>

#include <boost/preprocessor/control/iif.hpp>
#include <boost/predef/os/windows.h>
#include <boost/asio/signal_set.hpp>
#include <boost/vmd/is_number.hpp>
#include <boost/predef/os/macos.h>
#include <boost/scope_exit.hpp>
#include <boost/vmd/empty.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/actor.hpp>

#if BOOST_OS_WINDOWS
# if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
#  include <condition_variable>
#  include <atomic>
#  include <thread>
#  include <mutex>
#  include <deque>
# endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
#else // BOOST_OS_WINDOWS
# include <boost/asio/posix/stream_descriptor.hpp>
#endif // BOOST_OS_WINDOWS

#if BOOST_OS_UNIX
#include <sys/mman.h>
#endif // BOOST_OS_UNIX

#if BOOST_OS_LINUX
#include <linux/securebits.h>
#include <sys/capability.h>
#include <grp.h>
#endif // BOOST_OS_LINUX
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char system_key;

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
static char system_signal_key;
static char system_signal_set_mt_key;
static char system_signal_set_wait_key;

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static char system_in_key;
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static char system_out_key;
static char system_err_key;

#if BOOST_OS_LINUX
char linux_capabilities_mt_key;
#endif // BOOST_OS_LINUX

#if !BOOST_OS_WINDOWS
int system_spawn(lua_State* L);
void init_system_spawn(lua_State* L);
#endif // !BOOST_OS_WINDOWS
EMILUA_GPERF_DECLS_END(system);

#if BOOST_OS_WINDOWS
# if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
// Windows cannot read from STD_INPUT_HANDLE using overlapped IO. Therefore we
// use a thread.
struct stdin_service: public pending_operation
{
    struct waiting_fiber
    {
        lua_State* fiber;
        std::shared_ptr<unsigned char[]> buffer;
        std::size_t buffer_size;
    };

    stdin_service(vm_context& vm_ctx);

    void cancel() noexcept override
    {
        run = false;

        {
            std::unique_lock<std::mutex> lk{queue_mtx};
            queue.clear();
            queue_is_not_empty_cond.notify_all(); //< forced spurious wakeup
        }

        lua_State* fiber = current_job;
        lua_State* expected = fiber;
        if (fiber && current_job.compare_exchange_strong(expected, NULL)) {
            do {
                expected = fiber;
                CancelSynchronousIo(thread.native_handle());
            } while (!current_job.compare_exchange_weak(expected, NULL));
        }

        thread.join();
    }

    std::deque<waiting_fiber> queue;
    std::mutex queue_mtx;
    std::condition_variable queue_is_not_empty_cond;
    std::atomic<lua_State*> current_job = nullptr;

    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
    std::shared_ptr<vm_context> vm_ctx;

    std::atomic_bool run = true;
    std::thread thread;
};

inline stdin_service::stdin_service(vm_context& vm_ctx)
    : pending_operation{/*shared_ownership=*/false}
    , work_guard{vm_ctx.work_guard()}
    , thread{[this]() {
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        while (run) {
            stdin_service::waiting_fiber fiber;
            {
                std::unique_lock<std::mutex> lk{queue_mtx};
                if (queue.empty())
                    this->vm_ctx.reset();
                while (queue.empty()) {
                    if (!run)
                        return;
                    queue_is_not_empty_cond.wait(lk);
                }
                assert(this->vm_ctx);

                fiber.fiber = queue.front().fiber;
                fiber.buffer = std::move(queue.front().buffer);
                fiber.buffer_size = queue.front().buffer_size;

                current_job = fiber.fiber;
                queue.pop_front();
            }

            DWORD numberOfBytesRead;
            BOOL ok = ReadFile(hStdin, fiber.buffer.get(), fiber.buffer_size,
                               &numberOfBytesRead, /*lpOverlapped=*/NULL);
            boost::system::error_code ec;
            if (!ok) {
                DWORD last_error = GetLastError();
                ec = boost::system::error_code(
                    last_error, asio::error::get_system_category());
            }

            lua_State* expected = fiber.fiber;
            if (!current_job.compare_exchange_strong(expected, NULL)) {
                assert(expected == NULL);
                current_job = fiber.fiber;
            }

            auto& v = this->vm_ctx;
            this->vm_ctx->strand().post(
                [vm_ctx=v,ec,numberOfBytesRead,fiber=fiber.fiber]() {
                    vm_ctx->fiber_resume(
                        fiber,
                        hana::make_set(
                            vm_context::options::auto_detect_interrupt,
                            hana::make_pair(
                                vm_context::options::arguments,
                                hana::make_tuple(ec, numberOfBytesRead))));
                }, std::allocator<void>{});
        }
    }}
{}
# endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static int system_out_write_some(lua_State* L)
{
    // we don't really need to check for suspend-allowed here given no
    // fiber-switch happens and we block the whole thread, but it's better to do
    // it anyway to guarantee the function will behave the same across different
    // platforms
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

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

    DWORD numberOfBytesWritten;
    BOOL ok = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), bs->data.get(),
                        bs->size, &numberOfBytesWritten, /*lpOverlapped=*/NULL);
    if (!ok) {
        boost::system::error_code ec(
            GetLastError(), asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushinteger(L, numberOfBytesWritten);
    return 1;
}

static int system_err_write_some(lua_State* L)
{
    // we don't really need to check for suspend-allowed here given no
    // fiber-switch happens and we block the whole thread, but it's better to do
    // it anyway to guarantee the function will behave the same across different
    // platforms
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

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

    DWORD numberOfBytesWritten;
    BOOL ok = WriteFile(GetStdHandle(STD_ERROR_HANDLE), bs->data.get(),
                        bs->size, &numberOfBytesWritten, /*lpOverlapped=*/NULL);
    if (!ok) {
        boost::system::error_code ec(
            GetLastError(), asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushinteger(L, numberOfBytesWritten);
    return 1;
}
#else // BOOST_OS_WINDOWS
struct stdstream_service: public pending_operation
{
    stdstream_service(asio::io_context& ioctx)
        : pending_operation{/*shared_ownership=*/false}
        , in{ioctx, STDIN_FILENO}
        , out{ioctx, STDOUT_FILENO}
        , err{ioctx, STDERR_FILENO}
    {}

    ~stdstream_service()
    {
        in.release();
        out.release();
        err.release();
    }

    void cancel() noexcept override
    {}

    asio::posix::stream_descriptor in;
    asio::posix::stream_descriptor out;
    asio::posix::stream_descriptor err;
};
#endif // BOOST_OS_WINDOWS

static int system_signal_set_new(lua_State* L)
{
    int nargs = lua_gettop(L);

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    for (int i = 1 ; i <= nargs ; ++i) {
        if (lua_type(L, i) != LUA_TNUMBER) {
            push(L, std::errc::invalid_argument, "arg", i);
            return lua_error(L);
        }
    }

    auto set = static_cast<asio::signal_set*>(
        lua_newuserdata(L, sizeof(asio::signal_set))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    setmetatable(L, -2);
    new (set) asio::signal_set{vm_ctx.strand().context()};

    for (int i = 1 ; i <= nargs ; ++i) {
        boost::system::error_code ec;
        set->add(lua_tointeger(L, i), ec);
        if (ec) {
            push(L, ec, "arg", i);
            return lua_error(L);
        }
    }

    return 1;
}

static int system_signal_set_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    set->async_wait(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec,
                                   int signal_number) {
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args, hana::make_tuple(ec, signal_number))));
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(signal_set)
EMILUA_GPERF_NAMESPACE(emilua)
static int system_signal_set_add(lua_State* L)
{
    lua_settop(L, 2);
    auto& vm_ctx = get_vm_context(L);

    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->add(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_remove(lua_State* L)
{
    lua_settop(L, 2);

    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    if (lua_type(L, 2) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->remove(lua_tointeger(L, 2), ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_clear(lua_State* L)
{
    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->clear(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

static int system_signal_set_cancel(lua_State* L)
{
    auto set = static_cast<asio::signal_set*>(lua_touserdata(L, 1));
    if (!set || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    set->cancel(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}
EMILUA_GPERF_DECLS_END(signal_set)

static int system_signal_set_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "wait",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &system_signal_set_wait_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, system_signal_set_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "add",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, system_signal_set_add);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "remove",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, system_signal_set_remove);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "clear",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, system_signal_set_clear);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int system_signal_raise(lua_State* L)
{
    int signal_number = luaL_checkinteger(L, 1);

#define EMILUA_DETAIL_IS_SIG_X_OR(SIG) signal_number == SIG ||
#define EMILUA_IS_SIG_X_OR(SIG) BOOST_PP_IIF( \
        BOOST_VMD_IS_NUMBER(SIG), EMILUA_DETAIL_IS_SIG_X_OR, BOOST_VMD_EMPTY \
    )(SIG)

#define EMILUA_DETAIL_IS_NOT_SIG_X_AND(SIG) signal_number != SIG &&
#define EMILUA_IS_NOT_SIG_X_AND(SIG) BOOST_PP_IIF( \
        BOOST_VMD_IS_NUMBER(SIG), \
        EMILUA_DETAIL_IS_NOT_SIG_X_AND, BOOST_VMD_EMPTY \
    )(SIG)

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        // SIGKILL and SIGSTOP are the only signals that cannot be caught,
        // blocked, or ignored. If we allowed any child VM to raise these
        // signals, then the protection to only allow the main VM to force-exit
        // the process would be moot.
        if (
            EMILUA_IS_SIG_X_OR(SIGKILL)
            EMILUA_IS_SIG_X_OR(SIGSTOP)
            false
        ) {
            push(L, std::errc::operation_not_permitted);
            return lua_error(L);
        }

        // Unless the main VM has a handler installed (the check doesn't need to
        // be race-free... that's not a problem) for the process-terminating
        // signal, forbid slave VMs from raising it.
        if (
            // Default action is to continue the process (whatever lol)
            EMILUA_IS_NOT_SIG_X_AND(SIGCONT)

            // Default action is to ignore the signal
            EMILUA_IS_NOT_SIG_X_AND(SIGCHLD)
            EMILUA_IS_NOT_SIG_X_AND(SIGURG)
            EMILUA_IS_NOT_SIG_X_AND(SIGWINCH)

            true
        ) {
#ifdef _POSIX_C_SOURCE
            struct sigaction curact;
            if (
                sigaction(signal_number, /*act=*/NULL, /*oldact=*/&curact) == -1
            ) {
                push(L, std::error_code{errno, std::system_category()});
                return lua_error(L);
            }

            if (curact.sa_handler == SIG_DFL) {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            }
#else
            // TODO: a Windows implementation that checks for SIG_DFL
            push(L, std::errc::operation_not_permitted);
            return lua_error(L);
#endif // _POSIX_C_SOURCE
        }
    }

#undef EMILUA_IS_NOT_SIG_X_AND
#undef EMILUA_DETAIL_IS_NOT_SIG_X_AND
#undef EMILUA_IS_SIG_X_OR
#undef EMILUA_DETAIL_IS_SIG_X_OR

    int ret = std::raise(signal_number);
    if (ret != 0) {
        push(L, errc::raise_error, "ret", ret);
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int system_signal_ignore(lua_State* L)
{
    int signo = luaL_checkinteger(L, 1);
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    struct sigaction sa;
    if (sigaction(signo, /*act=*/NULL, /*oldact=*/&sa) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    // do not interfere with Boost.Asio installed signal handlers
    if (sa.sa_handler != SIG_DFL && sa.sa_handler != SIG_IGN) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(signo, /*act=*/&sa, /*oldact=*/NULL) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}

static int system_signal_default(lua_State* L)
{
    int signo = luaL_checkinteger(L, 1);
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    struct sigaction sa;
    if (sigaction(signo, /*act=*/NULL, /*oldact=*/&sa) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    // do not interfere with Boost.Asio installed signal handlers
    if (sa.sa_handler != SIG_DFL && sa.sa_handler != SIG_IGN) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(signo, /*act=*/&sa, /*oldact=*/NULL) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_UNIX

#if BOOST_OS_WINDOWS
# if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
static int system_in_read_some(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

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

    stdin_service::waiting_fiber operation;
    operation.fiber = vm_ctx.current_fiber();
    operation.buffer = bs->data;
    operation.buffer_size = bs->size;

    stdin_service* service = nullptr;

    for (auto& op: vm_ctx.pending_operations) {
        service = dynamic_cast<stdin_service*>(&op);
        if (service) {
            std::unique_lock<std::mutex> lk(service->queue_mtx);
            if (service->queue.empty() && !service->vm_ctx)
                service->vm_ctx = vm_ctx.shared_from_this();
            service->queue.push_back(operation);

            if (service->queue.size() == 1)
                service->queue_is_not_empty_cond.notify_all();
            break;
        }
    }

    if (!service) {
        service = new stdin_service{vm_ctx};
        vm_ctx.pending_operations.push_back(*service);

        std::unique_lock<std::mutex> lk{service->queue_mtx};
        service->vm_ctx = vm_ctx.shared_from_this();
        service->queue.push_back(operation);
        service->queue_is_not_empty_cond.notify_all();
    }

    lua_pushlightuserdata(L, service);
    lua_pushlightuserdata(L, vm_ctx.current_fiber());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<stdin_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto fiber = static_cast<lua_State*>(
                lua_touserdata(L, lua_upvalueindex(2)));

            auto vm_ctx = get_vm_context(L).shared_from_this();

            {
                std::unique_lock<std::mutex> lk(service->queue_mtx);
                auto it = service->queue.begin();
                auto end = service->queue.end();
                for (; it != end ; ++it) {
                    if (it->fiber == fiber) {
                        service->queue.erase(it);
                        vm_ctx->strand().post(
                            [vm_ctx,fiber]() {
                                auto ec = make_error_code(errc::interrupted);
                                vm_ctx->fiber_resume(
                                    fiber,
                                    hana::make_set(
                                        hana::make_pair(
                                            vm_context::options::arguments,
                                            hana::make_tuple(ec))));
                            }, std::allocator<void>{});
                        return 0;
                    }
                }

                lua_State* expected = fiber;
                if (
                    service->current_job.compare_exchange_strong(expected, NULL)
                ) {
                    do {
                        expected = fiber;
                        CancelSynchronousIo(service->thread.native_handle());
                    } while (!service->current_job.compare_exchange_weak(
                        expected, NULL
                    ));
                }
            }

            return 0;
        },
        2);
    set_interrupter(L, vm_ctx);

    return lua_yield(L, 0);
}
# endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
#else // BOOST_OS_WINDOWS
static int system_in_read_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

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

    stdstream_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<stdstream_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new stdstream_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    service->in.async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                std::error_code ec2 = ec;
                if (ec2 == std::errc::interrupted)
                    ec2 = errc::interrupted;
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec2, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int system_out_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

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

    stdstream_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<stdstream_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new stdstream_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    service->out.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                std::error_code ec2 = ec;
                if (ec2 == std::errc::interrupted)
                    ec2 = errc::interrupted;
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec2, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}

static int system_err_write_some(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

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

    stdstream_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<stdstream_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new stdstream_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    service->err.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                std::error_code ec2 = ec;
                if (ec2 == std::errc::interrupted)
                    ec2 = errc::interrupted;
                boost::ignore_unused(buf);
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec2, bytes_transferred)))
                );
            }
        ))
    );

    return lua_yield(L, 0);
}
#endif // BOOST_OS_WINDOWS

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
inline int system_arguments(lua_State* L)
{
    auto& appctx = get_vm_context(L).appctx;
    lua_createtable(L, /*narr=*/appctx.app_args.size(), /*nrec=*/0);
    int n = 0;
    for (auto& arg: appctx.app_args) {
        push(L, arg);
        lua_rawseti(L, -2, ++n);
    }
    return 1;
}

inline int system_environment(lua_State* L)
{
    auto& appctx = get_vm_context(L).appctx;
    lua_createtable(L, /*narr=*/0, /*nrec=*/appctx.app_env.size());
    for (auto& [key, value]: appctx.app_env) {
        push(L, key);
        push(L, value);
        lua_rawset(L, -3);
    }
    return 1;
}
EMILUA_GPERF_DECLS_END(system)

#if BOOST_OS_UNIX
template<int FD>
static int system_stdhandle_dup(lua_State* L)
{
    int newfd = dup(FD);
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

template<int FD>
static int system_stdhandle_isatty(lua_State* L)
{
    lua_pushboolean(L, isatty(FD));
    return 1;
}

template<int FD>
static int system_stdhandle_tcgetpgrp(lua_State* L)
{
    pid_t res = tcgetpgrp(FD);
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    lua_pushnumber(L, res);
    return 1;
}

template<int FD>
static int system_stdhandle_tcsetpgrp(lua_State* L)
{
    if (tcsetpgrp(FD, luaL_checknumber(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_UNIX

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
inline int system_in(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_in_key);
    return 1;
}
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

inline int system_out(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_out_key);
    return 1;
}

inline int system_err(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_err_key);
    return 1;
}

inline int system_signal(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &system_signal_key);
    return 1;
}

static int system_exit(lua_State* L)
{
    lua_settop(L, 2);

    int exit_code = luaL_optint(L, 1, EXIT_SUCCESS);

    auto& vm_ctx = get_vm_context(L);
    if (vm_ctx.is_master()) {
        if (lua_type(L, 2) == LUA_TTABLE) {
            lua_getfield(L, 2, "force");
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                break;
            case LUA_TNUMBER:
                switch (lua_tointeger(L, -1)) {
                case 0:
                    break;
                case 1:
                    push(L, std::errc::not_supported);
                    return lua_error(L);
                case 2:
#if BOOST_OS_MACOS
                    std::_Exit(exit_code);
#else // BOOST_OS_MACOS
                    std::quick_exit(exit_code);
#endif // BOOST_OS_MACOS
                default:
                    push(L, std::errc::invalid_argument, "arg", "force");
                    return lua_error(L);
                }
                break;
            case LUA_TSTRING: {
                auto force = tostringview(L);
                if (force == "abort") {
                    std::abort();
                } else {
                    push(L, std::errc::invalid_argument, "arg", "force");
                    return lua_error(L);
                }
            }
            default:
                push(L, std::errc::invalid_argument, "arg", 2);
                return lua_error(L);
            }
        }

        vm_ctx.appctx.exit_code = exit_code;
    } else if (lua_type(L, 2) != LUA_TNIL) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    vm_ctx.notify_exit_request();
    return lua_yield(L, 0);
}

#if BOOST_OS_UNIX
static int system_getresuid(lua_State* L)
{
    uid_t ruid, euid, suid;
    int res = getresuid(&ruid, &euid, &suid);
    assert(res == 0);
    boost::ignore_unused(res);
    lua_pushinteger(L, ruid);
    lua_pushinteger(L, euid);
    lua_pushinteger(L, suid);
    return 3;
}

static int system_getresgid(lua_State* L)
{
    gid_t rgid, egid, sgid;
    int res = getresgid(&rgid, &egid, &sgid);
    assert(res == 0);
    boost::ignore_unused(res);
    lua_pushinteger(L, rgid);
    lua_pushinteger(L, egid);
    lua_pushinteger(L, sgid);
    return 3;
}

static int system_setresuid(lua_State* L)
{
    lua_settop(L, 3);
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }

    uid_t ruid = lua_tointeger(L, 1);
    uid_t euid = lua_tointeger(L, 2);
    uid_t suid = lua_tointeger(L, 3);
    if (setresuid(ruid, euid, suid) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::SETRESUID;
        request.resuid[0] = ruid;
        request.resuid[1] = euid;
        request.resuid[2] = suid;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int system_setresgid(lua_State* L)
{
    lua_settop(L, 3);
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }

    gid_t rgid = lua_tointeger(L, 1);
    gid_t egid = lua_tointeger(L, 2);
    gid_t sgid = lua_tointeger(L, 3);
    if (setresgid(rgid, egid, sgid) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::SETRESGID;
        request.resgid[0] = rgid;
        request.resgid[1] = egid;
        request.resgid[2] = sgid;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int system_getgroups(lua_State* L)
{
    std::vector<gid_t> ret;
    int len = -1;

    // other threads could be modifying the list
    // so we loop until it succeeds
    while (len == -1) {
        len = getgroups(0, NULL);
        ret.resize(len);
        len = getgroups(len, ret.data());
    }

    lua_createtable(L, /*narr=*/len, /*nrec=*/0);
    for (int i = 0 ; i != len ; ++i) {
        lua_pushinteger(L, ret[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int system_setgroups(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<gid_t> groups;
    for (int i = 0 ;; ++i) {
        lua_rawgeti(L, 1, i + 1);
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            goto input_ready;
        case LUA_TNUMBER:
            groups.emplace_back(lua_tointeger(L, -1));
            lua_pop(L, 1);
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
    }

 input_ready:
    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };

    int mfd = -1;
    BOOST_SCOPE_EXIT_ALL(&) { if (mfd != -1) close(mfd); };

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        if (groups.size() != 0) {
#if BOOST_OS_LINUX
            mfd = memfd_create("emilua/setgroups", /*flags=*/0);
#else
            mfd = shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
#endif // BOOST_OS_LINUX
            if (mfd == -1) {
                push(L, std::error_code{errno, std::system_category()});
                return lua_error(L);
            }

            if (ftruncate(mfd, sizeof(gid_t) * groups.size()) == -1) {
                push(L, std::error_code{errno, std::system_category()});
                return lua_error(L);
            }

            write(mfd, groups.data(), sizeof(gid_t) * groups.size());
        }
    }

    if (setgroups(groups.size(), groups.data()) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::SETGROUPS;
        request.setgroups_ngroups = static_cast<int>(groups.size());

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int) * 2)];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = (
            (groups.size() != 0) ?
            CMSG_SPACE(sizeof(int) * 2) : CMSG_SPACE(sizeof(int)));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = (
            (groups.size() != 0) ?
            CMSG_LEN(sizeof(int) * 2) : CMSG_LEN(sizeof(int)));

        {
            char* begin = (char*)CMSG_DATA(cmsg);
            char* it = begin;

            std::memcpy(it, &channel[1], sizeof(int));
            it += sizeof(int);

            if (groups.size() != 0)
                std::memcpy(it, &mfd, sizeof(int));
        }

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int system_getpid(lua_State* L)
{
    lua_pushinteger(L, getpid());
    return 1;
}

static int system_getppid(lua_State* L)
{
    lua_pushinteger(L, getppid());
    return 1;
}

static int system_kill(lua_State* L)
{
    lua_settop(L, 2);

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    if (kill(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}

static int system_getpgrp(lua_State* L)
{
    lua_pushinteger(L, getpgrp());
    return 1;
}

static int system_getpgid(lua_State* L)
{
    pid_t res = getpgid(luaL_checkinteger(L, 1));
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    lua_pushinteger(L, res);
    return 1;
}

static int system_setpgid(lua_State* L)
{
    lua_settop(L, 2);

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    if (setpgid(luaL_checkinteger(L, 1), luaL_checkinteger(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}

static int system_getsid(lua_State* L)
{
    pid_t res = getsid(luaL_checknumber(L, 1));
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    lua_pushinteger(L, res);
    return 1;
}

static int system_setsid(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    pid_t res = setsid();
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    lua_pushinteger(L, res);
    return 1;
}
#endif // BOOST_OS_UNIX
EMILUA_GPERF_DECLS_END(system)

EMILUA_GPERF_DECLS_BEGIN(linux_capabilities)
EMILUA_GPERF_NAMESPACE(emilua)
#if BOOST_OS_LINUX
static int linux_capabilities_dup(lua_State* L)
{
    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto& caps2 = *static_cast<cap_t*>(lua_newuserdata(L, sizeof(cap_t)));
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    setmetatable(L, -2);
    caps2 = cap_dup(*caps);
    return 1;
}

static int linux_capabilities_clear(lua_State* L)
{
    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    cap_clear(*caps);
    return 0;
}

static int linux_capabilities_clear_flag(lua_State* L)
{
    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    cap_flag_t flag;
    auto s = tostringview(L, 2);
    if (s == "effective") {
        flag = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        flag = CAP_INHERITABLE;
    } else if (s == "permitted") {
        flag = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    cap_clear_flag(*caps, flag);
    return 0;
}

static int linux_capabilities_get_flag(lua_State* L)
{
    lua_settop(L, 3);

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    const char* name = luaL_checkstring(L, 2);
    cap_value_t cap;
    if (cap_from_name(name, &cap) == -1) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    cap_flag_t flag;
    auto s = tostringview(L, 3);
    if (s == "effective") {
        flag = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        flag = CAP_INHERITABLE;
    } else if (s == "permitted") {
        flag = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    cap_flag_value_t value;
    cap_get_flag(*caps, cap, flag, &value);
    if (value == CAP_SET)
        lua_pushboolean(L, 1);
    else
        lua_pushboolean(L, 0);
    return 1;
}

static int linux_capabilities_set_flag(lua_State* L)
{
    lua_settop(L, 4);
    luaL_checktype(L, 3, LUA_TTABLE);
    luaL_checktype(L, 4, LUA_TBOOLEAN);

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    cap_flag_t flag;
    auto s = tostringview(L, 2);
    if (s == "effective") {
        flag = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        flag = CAP_INHERITABLE;
    } else if (s == "permitted") {
        flag = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::vector<cap_value_t> values;
    for (int i = 1 ;; ++i) {
        lua_rawgeti(L, 3, i);
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            goto end_for;
        case LUA_TSTRING: {
            const char* name = lua_tostring(L, -1);
            cap_value_t cap;
            if (cap_from_name(name, &cap) == -1) {
                push(L, std::errc::invalid_argument, "arg", 3);
                return lua_error(L);
            }
            values.emplace_back(cap);
            lua_pop(L, 1);
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
    }
 end_for:

    cap_flag_value_t value = lua_toboolean(L, 4) ? CAP_SET : CAP_CLEAR;

    if (cap_set_flag(*caps, flag, values.size(), values.data(), value) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

static int linux_capabilities_fill_flag(lua_State* L)
{
    lua_settop(L, 4);

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    cap_flag_t to;
    auto s = tostringview(L, 2);
    if (s == "effective") {
        to = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        to = CAP_INHERITABLE;
    } else if (s == "permitted") {
        to = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto caps2 = static_cast<cap_t*>(lua_touserdata(L, 3));
    if (!caps2 || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    cap_flag_t from;
    s = tostringview(L, 4);
    if (s == "effective") {
        from = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        from = CAP_INHERITABLE;
    } else if (s == "permitted") {
        from = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 4);
        return lua_error(L);
    }

    cap_fill_flag(*caps, to, *caps2, from);
    return 0;
}

static int linux_capabilities_fill(lua_State* L)
{
    lua_settop(L, 3);

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    cap_flag_t to;
    auto s = tostringview(L, 2);
    if (s == "effective") {
        to = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        to = CAP_INHERITABLE;
    } else if (s == "permitted") {
        to = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    cap_flag_t from;
    s = tostringview(L, 3);
    if (s == "effective") {
        from = CAP_EFFECTIVE;
    } else if (s == "inheritable") {
        from = CAP_INHERITABLE;
    } else if (s == "permitted") {
        from = CAP_PERMITTED;
    } else {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    cap_fill(*caps, to, from);
    return 0;
}

static int linux_capabilities_set_proc(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };

    int mfd = -1;
    BOOST_SCOPE_EXIT_ALL(&) { if (mfd != -1) close(mfd); };

    ssize_t mfd_size;

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        char* str = cap_to_text(*caps, &mfd_size);
        BOOST_SCOPE_EXIT_ALL(&) { cap_free(str); };

        if (!str) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        ++mfd_size; //< include nul terminator

        mfd = memfd_create("emilua/cap_set_proc", /*flags=*/0);
        if (mfd == -1) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        if (ftruncate(mfd, mfd_size) == -1) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        write(mfd, str, mfd_size);
    }

    if (cap_set_proc(*caps) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::CAP_SET_PROC;
        request.cap_set_proc_mfd_size = mfd_size;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int) * 2)];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * 2);

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * 2);

        {
            char* begin = (char*)CMSG_DATA(cmsg);
            char* it = begin;

            std::memcpy(it, &channel[1], sizeof(int));
            it += sizeof(int);

            std::memcpy(it, &mfd, sizeof(int));
        }

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int linux_capabilities_get_nsowner(lua_State* L)
{
    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushinteger(L, cap_get_nsowner(*caps));
    return 1;
}

static int linux_capabilities_set_nsowner(lua_State* L)
{
    auto caps = static_cast<cap_t*>(lua_touserdata(L, 1));
    if (!caps || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (cap_set_nsowner(*caps, luaL_checkinteger(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_LINUX
EMILUA_GPERF_DECLS_END(linux_capabilities)

#if BOOST_OS_LINUX
static int linux_capabilities_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PPGUARD(BOOST_OS_LINUX)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "dup",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_dup);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "clear",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_clear);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "clear_flag",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_clear_flag);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "get_flag",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_get_flag);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "set_flag",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_set_flag);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "fill_flag",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_fill_flag);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "fill",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_fill);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "set_proc",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_set_proc);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "get_nsowner",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_get_nsowner);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "set_nsowner",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, linux_capabilities_set_nsowner);
                return 1;
            }
        )
    EMILUA_GPERF_END(key)(L);
}

static int linux_capabilities_mt_gc(lua_State* L)
{
    auto& caps = *static_cast<cap_t*>(lua_touserdata(L, 1));
    cap_free(caps);
    return 0;
}

static int linux_capabilities_mt_tostring(lua_State* L)
{
    auto& caps = *static_cast<cap_t*>(lua_touserdata(L, 1));
    ssize_t len;
    char* str = cap_to_text(caps, &len);
    BOOST_SCOPE_EXIT_ALL(&) { cap_free(str); };
    lua_pushlstring(L, str, len);
    return 1;
}
#endif // BOOST_OS_LINUX

EMILUA_GPERF_DECLS_BEGIN(linux_capabilities)
EMILUA_GPERF_NAMESPACE(emilua)
#if BOOST_OS_LINUX
static int system_cap_get_proc(lua_State* L)
{
    auto& caps = *static_cast<cap_t*>(lua_newuserdata(L, sizeof(cap_t)));
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    setmetatable(L, -2);
    caps = cap_get_proc();
    return 1;
}

static int system_cap_init(lua_State* L)
{
    auto& caps = *static_cast<cap_t*>(lua_newuserdata(L, sizeof(cap_t)));
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    setmetatable(L, -2);
    caps = cap_init();
    return 1;
}

static int system_cap_from_text(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);

    cap_t caps = cap_from_text(str);
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

static int system_cap_get_bound(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    cap_value_t cap;
    if (cap_from_name(name, &cap) == -1) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    int res = cap_get_bound(cap);
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    lua_pushboolean(L, res);
    return 1;
}

static int system_cap_drop_bound(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    const char* name = luaL_checkstring(L, 1);
    cap_value_t cap;
    if (cap_from_name(name, &cap) == -1) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }

    if (cap_drop_bound(cap) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::CAP_DROP_BOUND;
        request.cap_value = cap;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int system_cap_get_ambient(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    cap_value_t cap;
    if (cap_from_name(name, &cap) == -1) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    int res = cap_get_ambient(cap);
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    lua_pushboolean(L, res);
    return 1;
}

static int system_cap_set_ambient(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TBOOLEAN);

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    const char* name = luaL_checkstring(L, 1);
    cap_value_t cap;
    if (cap_from_name(name, &cap) == -1) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    cap_flag_value_t value = lua_toboolean(L, 2) ? CAP_SET : CAP_CLEAR;

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }

    if (cap_set_ambient(cap, value) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::CAP_SET_AMBIENT;
        request.cap_value = cap;
        request.cap_flag_value = value;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int system_cap_reset_ambient(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }

    if (cap_reset_ambient() == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::CAP_RESET_AMBIENT;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}

static int system_cap_get_secbits(lua_State* L)
{
    lua_pushinteger(L, cap_get_secbits());
    return 1;
}

static int system_cap_set_secbits(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    unsigned bits = luaL_checkinteger(L, 1);

    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }

    if (cap_set_secbits(bits) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::CAP_SET_SECBITS;
        request.cap_set_secbits_value = bits;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }

    return 0;
}
#endif // BOOST_OS_LINUX
EMILUA_GPERF_DECLS_END(linux_capabilities)

static int system_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR("environment", system_environment)
        EMILUA_GPERF_PAIR("signal", system_signal)
        EMILUA_GPERF_PAIR("arguments", system_arguments)
        EMILUA_GPERF_PAIR(
            "in_",
            [](lua_State* L) -> int {
#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
                return system_in(L);
#else // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
                return throw_enosys(L);
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
            })
        EMILUA_GPERF_PAIR("out", system_out)
        EMILUA_GPERF_PAIR("err", system_err)
        EMILUA_GPERF_PAIR(
            "spawn",
            [](lua_State* L) -> int {
#if BOOST_OS_WINDOWS
                lua_pushcfunction(L, throw_enosys);
#else // BOOST_OS_WINDOWS
                lua_pushcfunction(L, system_spawn);
#endif // BOOST_OS_WINDOWS
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_get_proc",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_get_proc);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_init",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_init);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_from_text",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_from_text);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_get_bound",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_get_bound);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_drop_bound",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_drop_bound);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_get_ambient",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_get_ambient);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_ambient",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_set_ambient);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_reset_ambient",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_reset_ambient);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_get_secbits",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_get_secbits);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_secbits",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, system_cap_set_secbits);
#else // BOOST_OS_LINUX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_NOROOT",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_NOROOT);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_NOROOT_LOCKED",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_NOROOT_LOCKED);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_SETUID_FIXUP",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_NO_SETUID_FIXUP);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_SETUID_FIXUP_LOCKED",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_NO_SETUID_FIXUP_LOCKED);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_KEEP_CAPS",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_KEEP_CAPS);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_KEEP_CAPS_LOCKED",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_KEEP_CAPS_LOCKED);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_CAP_AMBIENT_RAISE",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_NO_CAP_AMBIENT_RAISE);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushinteger(L, SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED);
                return 1;
#else // BOOST_OS_LINUX
                return throw_enosys(L);
#endif // BOOST_OS_LINUX
            })
        EMILUA_GPERF_PAIR(
            "getresuid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getresuid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getresgid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getresgid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setresuid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_setresuid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setresgid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_setresgid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getgroups",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getgroups);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setgroups",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_setgroups);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getpid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getpid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getppid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getppid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "kill",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_kill);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getpgrp",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getpgrp);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getpgid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getpgid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setpgid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_setpgid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "getsid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_getsid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "setsid",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, system_setsid);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "exit",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, system_exit);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

void init_system(lua_State* L)
{
    lua_pushlightuserdata(L, &system_key);
    {
        lua_newuserdata(L, /*size=*/1);

        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/2);

            lua_pushliteral(L, "__metatable");
            lua_pushliteral(L, "system");
            lua_rawset(L, -3);

            lua_pushliteral(L, "__index");
            lua_pushcfunction(L, system_mt_index);
            lua_rawset(L, -3);
        }

        setmetatable(L, -2);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_signal_key);
    {
        lua_newtable(L);

        lua_pushliteral(L, "raise");
        lua_pushcfunction(L, system_signal_raise);
        lua_rawset(L, -3);

        lua_pushliteral(L, "set");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/1);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, system_signal_set_new);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

#if BOOST_OS_UNIX
        lua_pushliteral(L, "ignore");
        lua_pushcfunction(L, system_signal_ignore);
        lua_rawset(L, -3);

        lua_pushliteral(L, "default");
        lua_pushcfunction(L, system_signal_default);
        lua_rawset(L, -3);
#endif // BOOST_OS_UNIX

#define EMILUA_DEF_SIGNAL(KEY, VALUE) do { \
            lua_pushliteral(L, KEY);       \
            lua_pushinteger(L, VALUE);     \
            lua_rawset(L, -3);             \
        } while(0)
#define EMILUA_DEF_SIGNAL2(SIG) EMILUA_DEF_SIGNAL(#SIG, SIG)

        // <signal.h>
        EMILUA_DEF_SIGNAL2(SIGABRT);
        EMILUA_DEF_SIGNAL2(SIGFPE);
        EMILUA_DEF_SIGNAL2(SIGILL);
        EMILUA_DEF_SIGNAL2(SIGINT);
        EMILUA_DEF_SIGNAL2(SIGSEGV);
        EMILUA_DEF_SIGNAL2(SIGTERM);

#define EMILUA_DEF_SIGNAL3(SIG) BOOST_PP_IIF( \
        BOOST_VMD_IS_NUMBER(SIG), EMILUA_DEF_SIGNAL, BOOST_VMD_EMPTY \
    )(#SIG, SIG)

        // Unix
        EMILUA_DEF_SIGNAL3(SIGALRM);
        EMILUA_DEF_SIGNAL3(SIGBUS);
        EMILUA_DEF_SIGNAL3(SIGCHLD);
        EMILUA_DEF_SIGNAL3(SIGCONT);
        EMILUA_DEF_SIGNAL3(SIGHUP);
        EMILUA_DEF_SIGNAL3(SIGIO);
        EMILUA_DEF_SIGNAL3(SIGKILL);
        EMILUA_DEF_SIGNAL3(SIGPIPE);
        EMILUA_DEF_SIGNAL3(SIGPROF);
        EMILUA_DEF_SIGNAL3(SIGQUIT);
        EMILUA_DEF_SIGNAL3(SIGSTOP);
        EMILUA_DEF_SIGNAL3(SIGSYS);
        EMILUA_DEF_SIGNAL3(SIGTRAP);
        EMILUA_DEF_SIGNAL3(SIGTSTP);
        EMILUA_DEF_SIGNAL3(SIGTTIN);
        EMILUA_DEF_SIGNAL3(SIGTTOU);
        EMILUA_DEF_SIGNAL3(SIGURG);
        EMILUA_DEF_SIGNAL3(SIGUSR1);
        EMILUA_DEF_SIGNAL3(SIGUSR2);
        EMILUA_DEF_SIGNAL3(SIGVTALRM);
        EMILUA_DEF_SIGNAL3(SIGWINCH);
        EMILUA_DEF_SIGNAL3(SIGXCPU);
        EMILUA_DEF_SIGNAL3(SIGXFSZ);

        // Windows
        EMILUA_DEF_SIGNAL3(SIGBREAK);

#undef EMILUA_DEF_SIGNAL3
#undef EMILUA_DEF_SIGNAL2
#undef EMILUA_DEF_SIGNAL
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

#if !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1
    lua_pushlightuserdata(L, &system_in_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/5);

        lua_pushliteral(L, "read_some");
        rawgetp(L, LUA_REGISTRYINDEX,
                &var_args__retval1_to_error__fwd_retval2__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, system_in_read_some);
        lua_call(L, 2, 1);
        lua_rawset(L, -3);

# if BOOST_OS_UNIX
        lua_pushliteral(L, "dup");
        lua_pushcfunction(L, system_stdhandle_dup<STDIN_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "isatty");
        lua_pushcfunction(L, system_stdhandle_isatty<STDIN_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcgetpgrp");
        lua_pushcfunction(L, system_stdhandle_tcgetpgrp<STDIN_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcsetpgrp");
        lua_pushcfunction(L, system_stdhandle_tcsetpgrp<STDIN_FILENO>);
        lua_rawset(L, -3);
# endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // !BOOST_OS_WINDOWS || EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 1

    lua_pushlightuserdata(L, &system_out_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/5);

        lua_pushliteral(L, "write_some");
#if BOOST_OS_WINDOWS
        lua_pushcfunction(L, system_out_write_some);
#else // BOOST_OS_WINDOWS
        rawgetp(L, LUA_REGISTRYINDEX,
                &var_args__retval1_to_error__fwd_retval2__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, system_out_write_some);
        lua_call(L, 2, 1);
#endif // BOOST_OS_WINDOWS
        lua_rawset(L, -3);

#if BOOST_OS_UNIX
        lua_pushliteral(L, "dup");
        lua_pushcfunction(L, system_stdhandle_dup<STDOUT_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "isatty");
        lua_pushcfunction(L, system_stdhandle_isatty<STDOUT_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcgetpgrp");
        lua_pushcfunction(L, system_stdhandle_tcgetpgrp<STDOUT_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcsetpgrp");
        lua_pushcfunction(L, system_stdhandle_tcsetpgrp<STDOUT_FILENO>);
        lua_rawset(L, -3);
#endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_err_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/5);

        lua_pushliteral(L, "write_some");
#if BOOST_OS_WINDOWS
        lua_pushcfunction(L, system_err_write_some);
#else // BOOST_OS_WINDOWS
        rawgetp(L, LUA_REGISTRYINDEX,
                &var_args__retval1_to_error__fwd_retval2__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, system_err_write_some);
        lua_call(L, 2, 1);
#endif // BOOST_OS_WINDOWS
        lua_rawset(L, -3);

#if BOOST_OS_UNIX
        lua_pushliteral(L, "dup");
        lua_pushcfunction(L, system_stdhandle_dup<STDERR_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "isatty");
        lua_pushcfunction(L, system_stdhandle_isatty<STDERR_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcgetpgrp");
        lua_pushcfunction(L, system_stdhandle_tcgetpgrp<STDERR_FILENO>);
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcsetpgrp");
        lua_pushcfunction(L, system_stdhandle_tcsetpgrp<STDERR_FILENO>);
        lua_rawset(L, -3);
#endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_signal_set_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "system.signal.set");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, system_signal_set_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::signal_set>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &system_signal_set_wait_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, system_signal_set_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

#if BOOST_OS_LINUX
    lua_pushlightuserdata(L, &linux_capabilities_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "linux_capabilities");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, linux_capabilities_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, linux_capabilities_mt_gc);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, linux_capabilities_mt_tostring);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // BOOST_OS_LINUX

#if !BOOST_OS_WINDOWS
    init_system_spawn(L);
#endif // !BOOST_OS_WINDOWS
}

} // namespace emilua

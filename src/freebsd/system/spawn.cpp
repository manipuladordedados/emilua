/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>
#include <emilua/async_base.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/scope_exit.hpp>

#include <sys/capsicum.h>
#include <sys/procdesc.h>
#include <sys/procctl.h>
#include <sched.h>

#include <emilua/file_descriptor.hpp>
#include <emilua/filesystem.hpp>

#include <boost/asio/posix/stream_descriptor.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
static char subprocess_mt_key;
static char subprocess_wait_key;

struct spawn_arguments_t
{
    struct errno_reply_t
    {
        int code;
    };
    static_assert(sizeof(errno_reply_t) <= PIPE_BUF);

    bool use_path;
    int closeonexecpipe;
    const char* program;
    int programfd;
    char** argv;
    char** envp;
    int proc_stdin;
    int proc_stdout;
    int proc_stderr;
    boost::container::small_vector<std::pair<int, int>, 7> extra_fds;

    std::optional<int> scheduler_policy;
    std::optional<int> scheduler_priority;
    bool start_new_session;
    int set_ctty;
    std::optional<pid_t> process_group;
    int foreground;
    uid_t ruid;
    uid_t euid;
    gid_t rgid;
    gid_t egid;
    std::optional<std::vector<gid_t>> extra_groups;
    std::optional<mode_t> umask;
    std::optional<std::string> working_directory;
    int working_directoryfd;
    std::optional<int> pdeathsig;
};

struct subprocess
{
    subprocess(int childpidfd)
        : pdfd{childpidfd}
        , status{std::in_place_type_t<void>{}}
    {}

    ~subprocess() { close(); }

    bool is_open() { return pdfd != -1; }

    void close()
    {
        if (pdfd == -1)
            return;

        ::close(pdfd);
        pdfd = -1;
    }

    int pdfd;
    lua_State* fiber = nullptr;
    result<int, void> status;
};

struct procdesc_service : public pending_operation
{
    procdesc_service(asio::io_context& ioctx)
        : pending_operation{/*shared_ownership=*/false}
        , kq{ioctx}
    {}

    void cancel() noexcept override
    {}

    void on_wait(
        std::shared_ptr<vm_context>& vm_ctx,
        const boost::system::error_code&
    ) {
        if (!vm_ctx->valid())
            return;

        tevent.resize(nwaiters);
        struct timespec timeout{0};
        int res = kevent(
            kq.native_handle(), NULL, 0, tevent.data(), tevent.size(),
            &timeout);
        assert(res != -1);

        for (int i = 0 ; i != res ; ++i) {
            assert(tevent[i].filter == EVFILT_PROCDESC);
            auto p = static_cast<subprocess*>(tevent[i].udata);

            p->status = static_cast<int>(tevent[i].data);
            p->close();
            --nwaiters;

            auto fiber = p->fiber;
            vm_ctx->strand().post([vm_ctx,fiber]() {
                vm_ctx->fiber_resume(fiber);
            }, std::allocator<void>{});
        }

        if (nwaiters > 0) {
            kq.async_wait(
                asio::posix::descriptor_base::wait_read,
                asio::bind_executor(
                    vm_ctx->strand_using_defer(),
                    [vm_ctx,this](const boost::system::error_code& ec) mutable {
                        on_wait(vm_ctx, ec);
                    }));
        } else {
            running = false;
        }
    }

    asio::posix::stream_descriptor kq;
    bool running = false;
    std::size_t nwaiters = 0;
    std::vector<struct kevent> tevent;
};
EMILUA_GPERF_DECLS_END(system)

static int subprocess_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!p->is_open()) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    if (p->fiber) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    procdesc_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<procdesc_service*>(&op);
    }

    if (!service) {
        int kq = kqueue();
        if (kq == -1) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        service = new procdesc_service{vm_ctx->strand().context()};
        boost::system::error_code ec;
        service->kq.assign(kq, ec);
        if (ec) {
            close(kq);
            delete service;
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        vm_ctx->pending_operations.push_back(*service);
    }

    struct kevent ev;
    EV_SET(&ev, p->pdfd, EVFILT_PROCDESC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, p);
    if (kevent(service->kq.native_handle(), &ev, 1, NULL, 0, NULL) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    ++service->nwaiters;

    if (!service->running) {
        service->running = true;
        service->kq.async_wait(
            asio::posix::descriptor_base::wait_read,
            asio::bind_executor(
                vm_ctx->strand_using_defer(),
                [vm_ctx,service](const boost::system::error_code& ec) mutable {
                    service->on_wait(vm_ctx, ec);
                }));
    }

    p->fiber = vm_ctx->current_fiber();

    lua_pushlightuserdata(L, service);
    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<procdesc_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto p = static_cast<subprocess*>(
                lua_touserdata(L, lua_upvalueindex(2)));
            auto fiber = p->fiber;

            auto vm_ctx = get_vm_context(L).shared_from_this();

            struct kevent ev;
            EV_SET(&ev, p->pdfd, EVFILT_PROCDESC, EV_DELETE, 0, 0, NULL);
            if (
                kevent(service->kq.native_handle(), &ev, 1, NULL, 0, NULL) == -1
            ) {
                // event already removed, just abort
                return 0;
            }

            --service->nwaiters;
            p->fiber = nullptr;
            vm_ctx->strand().post([vm_ctx,fiber]() {
                auto ec = make_error_code(errc::interrupted);
                vm_ctx->fiber_resume(
                    fiber,
                    hana::make_set(
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec))));
            }, std::allocator<void>{});

            return 0;
        },
        2);
    set_interrupter(L, *vm_ctx);

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(subprocess)
EMILUA_GPERF_NAMESPACE(emilua)
static int subprocess_kill(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (!p->is_open()) {
        push(L, std::errc::no_such_process);
        return lua_error(L);
    }

    if (pdkill(p->pdfd, lua_tointeger(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

inline int subprocess_exit_code(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->status) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    if (WIFEXITED(p->status.value())) {
        lua_pushinteger(L, WEXITSTATUS(p->status.value()));
        return 1;
    } else if (WIFSIGNALED(p->status.value())) {
        lua_pushinteger(L, 128 + WTERMSIG(p->status.value()));
        return 1;
    } else {
        assert(false);
        push(L, std::errc::state_not_recoverable);
        return lua_error(L);
    }
}

inline int subprocess_exit_signal(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->status) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    if (WIFSIGNALED(p->status.value())) {
        lua_pushinteger(L, WTERMSIG(p->status.value()));
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

inline int subprocess_pid(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->is_open()) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    pid_t childpid;
    int res = pdgetpid(p->pdfd, &childpid);
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    lua_pushinteger(L, childpid);
    return 1;
}
EMILUA_GPERF_DECLS_END(subprocess)

static int subprocess_mt_index(lua_State* L)
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
                rawgetp(L, LUA_REGISTRYINDEX, &subprocess_wait_key);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR(
            "kill",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, subprocess_kill);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR("exit_code", subprocess_exit_code)
        EMILUA_GPERF_PAIR("exit_signal", subprocess_exit_signal)
        EMILUA_GPERF_PAIR("pid", subprocess_pid)
    EMILUA_GPERF_END(key)(L);
}

[[noreturn]] static void system_spawn_child_main(void* a)
{
    auto args = static_cast<spawn_arguments_t*>(a);
    spawn_arguments_t::errno_reply_t reply;

    {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        for (int signo = 1 ; signo != NSIG ; ++signo) {
            sigaction(signo, /*act=*/&sa, /*oldact=*/NULL);
        }
    }

    if (args->scheduler_policy) {
        struct sched_param sp;
        sp.sched_priority = *args->scheduler_priority;
        if (sched_setscheduler(/*pid=*/0, *args->scheduler_policy, &sp) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    } else if (args->scheduler_priority) {
        struct sched_param sp;
        sp.sched_priority = *args->scheduler_priority;
        if (sched_setparam(/*pid=*/0, &sp) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    if (args->start_new_session && setsid() == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        std::exit(1);
    }

    if (args->process_group && setpgid(/*pid=*/0, *args->process_group) == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        std::exit(1);
    }

    if (
        args->extra_groups &&
        setgroups(args->extra_groups->size(), args->extra_groups->data()) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        std::exit(1);
    }

    if (args->rgid != (gid_t)(-1) || args->egid != (gid_t)(-1)) {
        if (args->egid == (gid_t)(-1)) {
            gid_t ignored_rgid, ignored_sgid;
            getresgid(&ignored_rgid, &args->egid, &ignored_sgid);
        }
        if (setresgid(args->rgid, args->egid, args->egid) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    if (args->ruid != (uid_t)(-1) || args->euid != (uid_t)(-1)) {
        if (args->euid == (uid_t)(-1)) {
            uid_t ignored_ruid, ignored_suid;
            getresuid(&ignored_ruid, &args->euid, &ignored_suid);
        }
        if (setresuid(args->ruid, args->euid, args->euid) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    if (args->umask) umask(*args->umask);

    if (
        args->pdeathsig &&
        procctl(P_PID, 0, PROC_PDEATHSIG_CTL, &*args->pdeathsig) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        std::exit(1);
    }

    if (
        args->working_directory && chdir(args->working_directory->data()) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        std::exit(1);
    }

    // operations on file descriptors that will not change the file descriptor
    // table for the process {{{

    if (
        args->working_directoryfd != -1 &&
        fchdir(args->working_directoryfd) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        std::exit(1);
    }

    if (args->set_ctty != -1) {
        if (ioctl(args->set_ctty, TIOCSCTTY, /*force=*/0) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    } else if (args->foreground != -1) {
        pid_t pgrp;
        assert(args->process_group);
        if (*args->process_group != 0) {
            pgrp = *args->process_group;
        } else {
            pgrp = getpgrp();
        }

        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGTTOU);
        sigprocmask(SIG_BLOCK, &set, /*oldset=*/NULL);

        if (tcsetpgrp(args->foreground, pgrp) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }

        sigprocmask(SIG_UNBLOCK, &set, /*oldset=*/NULL);
    }

    // }}}

    // operations on file descriptors that will change the file descriptor table
    // for the process {{{

    // first operations that only mutate the safe range (destination will not
    // override source file descriptors that may be used later)

    switch (args->proc_stdin) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        break;
    }
    case STDIN_FILENO:
        break;
    default:
        if (dup2(args->proc_stdin, STDIN_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    switch (args->proc_stdout) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        break;
    }
    case STDOUT_FILENO:
        break;
    default:
        if (dup2(args->proc_stdout, STDOUT_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    switch (args->proc_stderr) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        break;
    }
    case STDERR_FILENO:
        break;
    default:
        if (dup2(args->proc_stderr, STDERR_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    // now operations that might override fds that we would use later if we're
    // not careful

    // return a number > 9 that is not used by any source fd
    auto nextfd = [idx=10,&args]() mutable {
        for (;;++idx) {
            if (idx == args->closeonexecpipe)
                continue;

            if (idx == args->programfd)
                continue;

            bool is_busy = false;
            for (const auto& [_, src] : args->extra_fds) {
                if (idx == src) {
                    is_busy = true;
                    break;
                }
            }
            if (is_busy)
                continue;

            break;
        }
        return idx++;
    };

    // destination is reserved for fds<10, so start by relocating any source
    // file descriptor in the range [3,10)

    if (args->closeonexecpipe < 10) {
        int dst = nextfd();
        if (dup2(args->closeonexecpipe, dst) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        args->closeonexecpipe = dst;
    }

    if (args->programfd != -1 && args->programfd < 10) {
        int dst = nextfd();
        if (dup2(args->programfd, dst) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        args->programfd = dst;
    }

    for (auto& [_, src] : args->extra_fds) {
        if (src > 9)
            continue;

        int src2 = nextfd();
        if (dup2(src, src2) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
        src = src2;
    }

    // finally perform the operations that will destroy old fds in the range
    // [3,10)

    for (int i = 3 ; i != 10 ; ++i) {
        bool inherit = false;
        for (auto& [dst, src] : args->extra_fds) {
            if (dst != i)
                continue;

            if (dup2(src, dst) == -1) {
                reply.code = errno;
                write(args->closeonexecpipe, &reply, sizeof(reply));
                std::exit(1);
            }

            inherit = true;
            break;
        }
        if (!inherit)
            close(i);
    }

    // }}}

    // do all necessary bookkeeping to close range [10,inf) on exec()

    if (args->closeonexecpipe != 10) {
        if (dup2(args->closeonexecpipe, 10) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    {
        int oldflags = fcntl(10, F_GETFD);
        if (fcntl(10, F_SETFD, oldflags | FD_CLOEXEC) == -1) {
            reply.code = errno;
            write(10, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    if (args->programfd != -1) {
        if (args->programfd != 11 && dup2(args->programfd, 11) == -1) {
            reply.code = errno;
            write(10, &reply, sizeof(reply));
            std::exit(1);
        }

        int oldflags = fcntl(11, F_GETFD);
        if (fcntl(11, F_SETFD, oldflags | FD_CLOEXEC) == -1) {
            reply.code = errno;
            write(10, &reply, sizeof(reply));
            std::exit(1);
        }
    }

    unsigned int first = (args->programfd == -1) ? 11 : 12;
    if (close_range(first, /*last=*/UINT_MAX, /*flags=*/0) == -1) {
        reply.code = errno;
        write(10, &reply, sizeof(reply));
        std::exit(1);
    }

    if (args->programfd != -1) {
        fexecve(args->programfd, args->argv, args->envp);
    } else if (args->use_path) {
#if 1
        errno = ENOTSUP;
#else
        // we can't use environ as of now:
        // https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263265
        environ = args->envp;
        execvp(args->program, args->argv);
#endif
    } else {
        execve(args->program, args->argv, args->envp);
    }

    reply.code = errno;
    write(10, &reply, sizeof(reply));
    std::exit(1);
}

int system_spawn(lua_State* L)
{
    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    const int FILE_DESCRIPTOR_MT_INDEX = lua_gettop(L);

    std::string program;
    int programfd = -1;
    bool use_path;
    lua_getfield(L, 1, "program");
    switch (lua_type(L, -1)) {
    case LUA_TSTRING:
        program = tostringview(L);
#if 1
        // we can't use environ for execvp() as of now:
        // https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=263265
        push(L, std::errc::not_supported, "arg", "program");
        return lua_error(L);
#else
        use_path = true;
        break;
#endif
    case LUA_TUSERDATA: {
        auto fd = static_cast<file_descriptor_handle*>(lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "program");
            return lua_error(L);
        }
        if (lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            if (*fd == INVALID_FILE_DESCRIPTOR) {
                push(L, std::errc::device_or_resource_busy, "arg", "program");
                return lua_error(L);
            }

            lua_pop(L, 1);
            programfd = *fd;
            break;
        }

        auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, -2));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", "program");
            return lua_error(L);
        }
        lua_pop(L, 2);

        try {
            program = path->string();;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
        use_path = false;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "program");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::vector<std::string> arguments;
    lua_getfield(L, 1, "arguments");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for;
            case LUA_TSTRING:
                arguments.emplace_back(tostringview(L));
                lua_pop(L, 1);
                break;
            default:
                push(L, std::errc::invalid_argument, "arg", "arguments");
                return lua_error(L);
            }
        }
        end_for:
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "arguments");
        return lua_error(L);
    }
    lua_pop(L, 1);
    std::vector<char*> argumentsb;
    argumentsb.reserve(arguments.size() + 1);
    for (auto& a: arguments) {
        argumentsb.emplace_back(a.data());
    }
    argumentsb.emplace_back(nullptr);

    std::vector<std::string> environment;
    lua_getfield(L, 1, "environment");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            if (
                lua_type(L, -2) != LUA_TSTRING || lua_type(L, -1) != LUA_TSTRING
            ) {
                push(L, std::errc::invalid_argument, "arg", "environment");
                return lua_error(L);
            }

            environment.emplace_back(tostringview(L, -2));
            environment.back() += '=';
            environment.back() += tostringview(L, -1);
            lua_pop(L, 1);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "environment");
        return lua_error(L);
    }
    lua_pop(L, 1);
    std::vector<char*> environmentb;
    environmentb.reserve(environment.size() + 1);
    for (auto& e: environment) {
        environmentb.emplace_back(e.data());
    }
    environmentb.emplace_back(nullptr);

    int proc_stdin = -1;
    lua_getfield(L, 1, "stdin");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        proc_stdin = STDIN_FILENO;
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "stdin");
            return lua_error(L);
        }
        proc_stdin = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "stdin");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int proc_stdout = -1;
    lua_getfield(L, 1, "stdout");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        proc_stdout = STDOUT_FILENO;
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "stdout");
            return lua_error(L);
        }
        proc_stdout = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "stdout");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int proc_stderr = -1;
    lua_getfield(L, 1, "stderr");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        proc_stderr = STDERR_FILENO;
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "stderr");
            return lua_error(L);
        }
        proc_stderr = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "stderr");
        return lua_error(L);
    }
    lua_pop(L, 1);

    boost::container::small_vector<std::pair<int, int>, 7> extra_fds;
    lua_getfield(L, 1, "extra_fds");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        for (int i = 3 ; i != 10 ; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                break;
            case LUA_TUSERDATA: {
                auto handle = static_cast<file_descriptor_handle*>(
                    lua_touserdata(L, -1));
                if (!lua_getmetatable(L, -1)) {
                    push(L, std::errc::invalid_argument, "arg", "extra_fds");
                    return lua_error(L);
                }
                if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
                    push(L, std::errc::invalid_argument, "arg", "extra_fds");
                    return lua_error(L);
                }
                if (*handle == INVALID_FILE_DESCRIPTOR) {
                    push(L, std::errc::device_or_resource_busy,
                         "arg", "extra_fds");
                    return lua_error(L);
                }
                extra_fds.emplace_back(i, *handle);
                lua_pop(L, 2);
                break;
            }
            default:
                push(L, std::errc::invalid_argument, "arg", "extra_fds");
                return lua_error(L);
            }
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "extra_fds");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<int> scheduler_policy;
    std::optional<int> scheduler_priority;
    lua_getfield(L, 1, "scheduler");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, -1, "policy");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TSTRING: {
            auto policy = tostringview(L);
            if (policy == "other") {
                scheduler_policy.emplace(SCHED_OTHER);
            } else if (policy == "batch") {
                push(L, std::errc::not_supported, "arg", "scheduler.policy");
                return lua_error(L);
            } else if (policy == "idle") {
                push(L, std::errc::not_supported, "arg", "scheduler.policy");
                return lua_error(L);
            } else if (policy == "fifo") {
                scheduler_policy.emplace(SCHED_FIFO);
            } else if (policy == "rr") {
                scheduler_policy.emplace(SCHED_RR);
            } else {
                push(L, std::errc::invalid_argument, "arg", "scheduler.policy");
                return lua_error(L);
            }
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", "scheduler.policy");
            return lua_error(L);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "reset_on_fork");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (!scheduler_policy) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.reset_on_fork");
                return lua_error(L);
            }

            push(L, std::errc::not_supported, "arg", "scheduler.reset_on_fork");
            return lua_error(L);
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "scheduler.reset_on_fork");
            return lua_error(L);
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "priority");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TNUMBER:
            scheduler_priority.emplace(lua_tointeger(L, -1));
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", "scheduler.priority");
            return lua_error(L);
        }
        lua_pop(L, 1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "scheduler");
        return lua_error(L);
    }
    lua_pop(L, 1);

    if (scheduler_policy) {
        switch (*scheduler_policy) {
        case SCHED_OTHER:
            if (scheduler_priority && *scheduler_priority != 0) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.priority");
                return lua_error(L);
            }
            scheduler_priority.emplace(0);
            break;
        case SCHED_FIFO:
        case SCHED_RR:
            if (!scheduler_priority) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.priority");
                return lua_error(L);
            }

            if (
                (*scheduler_priority <
                 sched_get_priority_min(*scheduler_policy)) ||
                (*scheduler_priority >
                 sched_get_priority_max(*scheduler_policy))
            ) {
                push(L, std::errc::invalid_argument,
                     "arg", "scheduler.priority");
                return lua_error(L);
            }
        }
    }

    bool start_new_session = false;
    lua_getfield(L, 1, "start_new_session");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        start_new_session = lua_toboolean(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "start_new_session");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int set_ctty = -1;
    lua_getfield(L, 1, "set_ctty");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        if (!start_new_session) {
            push(L, std::errc::invalid_argument, "arg", "set_ctty");
            return lua_error(L);
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "set_ctty");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "set_ctty");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "set_ctty");
            return lua_error(L);
        }
        set_ctty = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "set_ctty");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<pid_t> process_group;
    lua_getfield(L, 1, "process_group");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        process_group.emplace(lua_tointeger(L, -1));
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "process_group");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int foreground = -1;
    lua_getfield(L, 1, "foreground");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING: {
        if (!process_group) {
            push(L, std::errc::invalid_argument, "arg", "foreground");
            return lua_error(L);
        }

        auto value = tostringview(L);
        if (value == "stdin") {
            if (proc_stdin != STDIN_FILENO) {
                push(L, std::errc::invalid_argument, "arg", "foreground");
                return lua_error(L);
            }

            foreground = STDIN_FILENO;
        } else if (value == "stdout") {
            if (proc_stdout != STDOUT_FILENO) {
                push(L, std::errc::invalid_argument, "arg", "foreground");
                return lua_error(L);
            }

            foreground = STDOUT_FILENO;
        } else if (value == "stderr") {
            if (proc_stderr != STDERR_FILENO) {
                push(L, std::errc::invalid_argument, "arg", "foreground");
                return lua_error(L);
            }

            foreground = STDERR_FILENO;
        } else {
            push(L, std::errc::invalid_argument, "arg", "foreground");
            return lua_error(L);
        }
        break;
    }
    case LUA_TUSERDATA: {
        if (!process_group) {
            push(L, std::errc::invalid_argument, "arg", "foreground");
            return lua_error(L);
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "foreground");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "foreground");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "foreground");
            return lua_error(L);
        }
        foreground = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "foreground");
        return lua_error(L);
    }
    lua_pop(L, 1);

    if (set_ctty != -1 && foreground != -1) {
        push(L, std::errc::invalid_argument, "arg", "set_ctty/foreground");
        return lua_error(L);
    }

    uid_t ruid = -1;
    lua_getfield(L, 1, "ruid");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        ruid = lua_tointeger(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "ruid");
        return lua_error(L);
    }
    lua_pop(L, 1);

    uid_t euid = -1;
    lua_getfield(L, 1, "euid");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        euid = lua_tointeger(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "euid");
        return lua_error(L);
    }
    lua_pop(L, 1);

    gid_t rgid = -1;
    lua_getfield(L, 1, "rgid");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        rgid = lua_tointeger(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "rgid");
        return lua_error(L);
    }
    lua_pop(L, 1);

    gid_t egid = -1;
    lua_getfield(L, 1, "egid");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        egid = lua_tointeger(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "egid");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<std::vector<gid_t>> extra_groups;
    lua_getfield(L, 1, "extra_groups");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        extra_groups.emplace();
        for (int i = 1 ;; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for2;
            case LUA_TNUMBER:
                extra_groups->emplace_back(lua_tointeger(L, -1));
                lua_pop(L, 1);
                break;
            default:
                push(L, std::errc::invalid_argument, "arg", "extra_groups");
                return lua_error(L);
            }
        }
        end_for2:
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "extra_groups");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<mode_t> umask;
    lua_getfield(L, 1, "umask");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        umask.emplace(lua_tointeger(L, -1));
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "umask");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<std::string> working_directory;
    int working_directoryfd = -1;
    lua_getfield(L, 1, "working_directory");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto fd = static_cast<file_descriptor_handle*>(lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "working_directory");
            return lua_error(L);
        }
        if (lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            if (*fd == INVALID_FILE_DESCRIPTOR) {
                push(L, std::errc::device_or_resource_busy,
                     "arg", "working_directory");
                return lua_error(L);
            }

            lua_pop(L, 1);
            working_directoryfd = *fd;
            break;
        }

        auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, -2));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", "working_directory");
            return lua_error(L);
        }
        lua_pop(L, 2);

        try {
            working_directory.emplace(path->string());
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "working_directory");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::optional<int> pdeathsig;
    lua_getfield(L, 1, "pdeathsig");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        pdeathsig.emplace(lua_tointeger(L, -1));
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "pdeathsig");
        return lua_error(L);
    }
    lua_pop(L, 1);

    bool pd_daemon = !cap_sandboxed();
    lua_getfield(L, 1, "pd_daemon");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TBOOLEAN:
        pd_daemon = lua_toboolean(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "pd_daemon");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        int res = close(pipefd[0]);
        boost::ignore_unused(res);
        if (pipefd[1] != -1) {
            res = close(pipefd[1]);
            boost::ignore_unused(res);
        }
    };

    spawn_arguments_t args;
    args.use_path = use_path;
    args.closeonexecpipe = pipefd[1];
    args.program = program.data();
    args.programfd = programfd;
    args.argv = argumentsb.data();
    args.envp = environmentb.data();
    args.proc_stdin = proc_stdin;
    args.proc_stdout = proc_stdout;
    args.proc_stderr = proc_stderr;
    args.extra_fds = extra_fds;
    args.scheduler_policy = scheduler_policy;
    args.scheduler_priority = scheduler_priority;
    args.start_new_session = start_new_session;
    args.set_ctty = set_ctty;
    args.process_group = process_group;
    args.foreground = foreground;
    args.ruid = ruid;
    args.euid = euid;
    args.rgid = rgid;
    args.egid = egid;
    args.extra_groups = std::move(extra_groups);
    args.umask = umask;
    args.working_directory = working_directory;
    args.working_directoryfd = working_directoryfd;
    args.pdeathsig = pdeathsig;

    int pdfork_flags = 0;
    if (pd_daemon)
        pdfork_flags |= PD_DAEMON;

    int pidfd = -1;
    pid_t childpid = pdfork(&pidfd, pdfork_flags);
    if (childpid == 0) {
        system_spawn_child_main(&args);
        __builtin_unreachable();
    }
    if (childpid == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        if (pidfd != -1) {
            int res = pdkill(pidfd, SIGKILL);
            boost::ignore_unused(res);
            res = close(pidfd);
            boost::ignore_unused(res);
        }
    };

    int res = close(pipefd[1]);
    boost::ignore_unused(res);
    pipefd[1] = -1;

    spawn_arguments_t::errno_reply_t reply;
    auto nread = read(pipefd[0], &reply, sizeof(reply));
    assert(nread != -1);
    if (nread != 0) {
        // exec() or pre-exec() failed
        push(L, std::error_code{reply.code, std::system_category()});
        return lua_error(L);
    }

    auto p = static_cast<subprocess*>(lua_newuserdata(L, sizeof(subprocess)));
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    setmetatable(L, -2);
    new (p) subprocess{pidfd};
    pidfd = -1;

    return 1;
}

void init_system_spawn(lua_State* L)
{
    lua_pushlightuserdata(L, &subprocess_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "subprocess");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, subprocess_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<subprocess>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &subprocess_wait_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, subprocess_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

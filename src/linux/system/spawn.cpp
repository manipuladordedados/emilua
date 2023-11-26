/* Copyright (c) 2021, 2022, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>
#include <emilua/async_base.hpp>

#include <boost/container/small_vector.hpp>
#include <boost/scope_exit.hpp>

#include <linux/close_range.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <grp.h>

#include <emilua/file_descriptor.hpp>
#include <emilua/filesystem.hpp>

#include <boost/asio/posix/stream_descriptor.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
extern char linux_capabilities_mt_key;

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
    std::optional<unsigned long> pdeathsig;
    int nsenter_user;
    int nsenter_mount;
    int nsenter_uts;
    int nsenter_ipc;
    int nsenter_net;
};

struct spawn_reaper
{
    spawn_reaper(asio::io_context& ctx, int childpidfd, pid_t childpid,
                 int signal_on_gcreaper)
        : waiter{
            std::make_shared<asio::posix::stream_descriptor>(ctx, childpidfd)}
        , childpid{childpid}
        , signal_on_gcreaper{signal_on_gcreaper}
    {}

    std::shared_ptr<asio::posix::stream_descriptor> waiter;
    pid_t childpid;
    int signal_on_gcreaper;
};

struct subprocess
{
    subprocess()
        : info{std::in_place_type_t<void>{}}
    {}

    ~subprocess()
    {
        if (!reaper)
            return;

        if (reaper->signal_on_gcreaper != 0) {
            int res = kill(reaper->childpid, reaper->signal_on_gcreaper);
            boost::ignore_unused(res);
        }
        reaper->waiter->async_wait(
            asio::posix::descriptor_base::wait_read,
            [waiter=reaper->waiter](const boost::system::error_code& /*ec*/) {
                siginfo_t i;
                int res = waitid(P_PIDFD, waiter->native_handle(), &i, WEXITED);
                boost::ignore_unused(res);
            });
    }

    std::optional<spawn_reaper> reaper;
    bool wait_in_progress = false;
    result<siginfo_t, void> info;
};
EMILUA_GPERF_DECLS_END(system)

static int subprocess_wait(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
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

    if (!p->reaper) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    if (p->wait_in_progress) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    p->wait_in_progress = true;

    lua_pushvalue(L, 1);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto p = static_cast<subprocess*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            boost::system::error_code ignored_ec;
            p->reaper->waiter->cancel(ignored_ec);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    p->reaper->waiter->async_wait(
        asio::posix::descriptor_base::wait_read,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [waiter=p->reaper->waiter,vm_ctx,current_fiber,p](
                const boost::system::error_code& ec
            ) {
                if (vm_ctx->valid())
                    p->wait_in_progress = false;

                if (ec) {
                    assert(ec == asio::error::operation_aborted);
                    auto opt_args = vm_context::options::arguments;
                    vm_ctx->fiber_resume(
                        current_fiber,
                        hana::make_set(
                            vm_context::options::fast_auto_detect_interrupt,
                            hana::make_pair(opt_args, hana::make_tuple(ec))));
                    return;
                }

                if (!vm_ctx->valid())
                    return;

                siginfo_t i;
                int res = waitid(P_PIDFD, waiter->native_handle(), &i, WEXITED);
                boost::ignore_unused(res);
                p->info = i;

                p->reaper = std::nullopt;

                vm_ctx->fiber_resume(current_fiber);
            }
        )
    );

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

    if (!p->reaper) {
        push(L, std::errc::no_such_process);
        return lua_error(L);
    }

    if (kill(p->reaper->childpid, lua_tointeger(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
}

static int subprocess_cap_get(lua_State* L)
{
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

    if (!p->reaper) {
        push(L, std::errc::no_such_process);
        return lua_error(L);
    }

    cap_t caps = cap_get_pid(p->reaper->childpid);
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

inline int subprocess_exit_code(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->info) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    switch (p->info.value().si_code) {
    default:
        assert(false);
        push(L, std::errc::state_not_recoverable);
        return lua_error(L);
    case CLD_EXITED:
        lua_pushinteger(L, p->info.value().si_status);
        return 1;
    case CLD_KILLED:
    case CLD_DUMPED:
        lua_pushinteger(L, 128 + p->info.value().si_status);
        return 1;
    }
}

inline int subprocess_exit_signal(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->info) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }
    switch (p->info.value().si_code) {
    default:
        lua_pushnil(L);
        return 1;
    case CLD_KILLED:
    case CLD_DUMPED:
        lua_pushinteger(L, p->info.value().si_status);
        return 1;
    }
}

inline int subprocess_pid(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->reaper) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    lua_pushinteger(L, p->reaper->childpid);
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
        EMILUA_GPERF_PAIR(
            "cap_get",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, subprocess_cap_get);
                return 1;
            }
        )
        EMILUA_GPERF_PAIR("exit_code", subprocess_exit_code)
        EMILUA_GPERF_PAIR("exit_signal", subprocess_exit_signal)
        EMILUA_GPERF_PAIR("pid", subprocess_pid)
    EMILUA_GPERF_END(key)(L);
}

EMILUA_GPERF_DECLS_BEGIN(spawn)
EMILUA_GPERF_NAMESPACE(emilua)
static int system_spawn_child_main(void* a)
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
            return 1;
        }
    } else if (args->scheduler_priority) {
        struct sched_param sp;
        sp.sched_priority = *args->scheduler_priority;
        if (sched_setparam(/*pid=*/0, &sp) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->start_new_session && setsid() == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (args->process_group && setpgid(/*pid=*/0, *args->process_group) == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (
        args->extra_groups &&
        setgroups(args->extra_groups->size(), args->extra_groups->data()) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (args->rgid != (gid_t)(-1) || args->egid != (gid_t)(-1)) {
        if (args->egid == (gid_t)(-1)) {
            gid_t ignored_rgid, ignored_sgid;
            getresgid(&ignored_rgid, &args->egid, &ignored_sgid);
        }
        if (setresgid(args->rgid, args->egid, args->egid) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
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
            return 1;
        }
    }

    if (args->umask) umask(*args->umask);

    if (args->pdeathsig && prctl(PR_SET_PDEATHSIG, *args->pdeathsig) == -1) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (
        args->working_directory && chdir(args->working_directory->data()) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    // operations on file descriptors that will not change the file descriptor
    // table for the process {{{

    if (
        args->working_directoryfd != -1 &&
        fchdir(args->working_directoryfd) == -1
    ) {
        reply.code = errno;
        write(args->closeonexecpipe, &reply, sizeof(reply));
        return 1;
    }

    if (args->set_ctty != -1) {
        if (ioctl(args->set_ctty, TIOCSCTTY, /*force=*/0) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
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
            return 1;
        }

        sigprocmask(SIG_UNBLOCK, &set, /*oldset=*/NULL);
    }

    if (args->nsenter_user != -1) {
        if (setns(args->nsenter_user, CLONE_NEWUSER) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->nsenter_mount != -1) {
        if (setns(args->nsenter_mount, CLONE_NEWNS) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->nsenter_uts != -1) {
        if (setns(args->nsenter_uts, CLONE_NEWUTS) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->nsenter_ipc != -1) {
        if (setns(args->nsenter_ipc, CLONE_NEWIPC) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->nsenter_net != -1) {
        if (setns(args->nsenter_net, CLONE_NEWNET) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
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
            return 1;
        }
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        break;
    }
    case STDIN_FILENO:
        break;
    default:
        if (dup2(args->proc_stdin, STDIN_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    switch (args->proc_stdout) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        break;
    }
    case STDOUT_FILENO:
        break;
    default:
        if (dup2(args->proc_stdout, STDOUT_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
    }

    switch (args->proc_stderr) {
    case -1: {
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        if (dup2(pipefd[1], STDERR_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
        }
        break;
    }
    case STDERR_FILENO:
        break;
    default:
        if (dup2(args->proc_stderr, STDERR_FILENO) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
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
            return 1;
        }
        args->closeonexecpipe = dst;
    }

    if (args->programfd != -1 && args->programfd < 10) {
        int dst = nextfd();
        if (dup2(args->programfd, dst) == -1) {
            reply.code = errno;
            write(args->closeonexecpipe, &reply, sizeof(reply));
            return 1;
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
            return 1;
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
                return 1;
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
            return 1;
        }
    }

    {
        int oldflags = fcntl(10, F_GETFD);
        if (fcntl(10, F_SETFD, oldflags | FD_CLOEXEC) == -1) {
            reply.code = errno;
            write(10, &reply, sizeof(reply));
            return 1;
        }
    }

    if (args->programfd != -1) {
        if (args->programfd != 11 && dup2(args->programfd, 11) == -1) {
            reply.code = errno;
            write(10, &reply, sizeof(reply));
            return 1;
        }

        int oldflags = fcntl(11, F_GETFD);
        if (fcntl(11, F_SETFD, oldflags | FD_CLOEXEC) == -1) {
            reply.code = errno;
            write(10, &reply, sizeof(reply));
            return 1;
        }
    }

    unsigned int first = (args->programfd == -1) ? 11 : 12;
    if (close_range(first, /*last=*/UINT_MAX, /*flags=*/0) == -1) {
        reply.code = errno;
        write(10, &reply, sizeof(reply));
        return 1;
    }

    if (args->programfd != -1)
        fexecve(args->programfd, args->argv, args->envp);
    else if (args->use_path)
        execvpe(args->program, args->argv, args->envp);
    else
        execve(args->program, args->argv, args->envp);

    reply.code = errno;
    write(10, &reply, sizeof(reply));
    return 1;
}

int system_spawn(lua_State* L)
{
    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    auto& vm_ctx = get_vm_context(L);
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    const int FILE_DESCRIPTOR_MT_INDEX = lua_gettop(L);

    std::string program;
    int programfd = -1;
    bool use_path;
    lua_getfield(L, 1, "program");
    switch (lua_type(L, -1)) {
    case LUA_TSTRING:
        program = tostringview(L);
        use_path = true;
        break;
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

    int signal_on_gcreaper = SIGTERM;
    lua_getfield(L, 1, "signal_on_gcreaper");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        signal_on_gcreaper = lua_tointeger(L, -1);
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "signal_on_gcreaper");
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
                scheduler_policy.emplace(SCHED_BATCH);
            } else if (policy == "idle") {
                scheduler_policy.emplace(SCHED_IDLE);
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

            if (!lua_toboolean(L, -1))
                break;

            *scheduler_policy |= SCHED_RESET_ON_FORK;
            break;
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
        switch (
            int policy = *scheduler_policy & ~SCHED_RESET_ON_FORK ; policy
        ) {
        case SCHED_OTHER:
        case SCHED_BATCH:
        case SCHED_IDLE:
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
                *scheduler_priority < sched_get_priority_min(policy) ||
                *scheduler_priority > sched_get_priority_max(policy)
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

    std::optional<unsigned long> pdeathsig;
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

    int nsenter_user = -1;
    lua_getfield(L, 1, "nsenter_user");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_user");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_user");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "nsenter_user");
            return lua_error(L);
        }
        nsenter_user = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "nsenter_user");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int nsenter_mount = -1;
    lua_getfield(L, 1, "nsenter_mount");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_mount");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_mount");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "nsenter_mount");
            return lua_error(L);
        }
        nsenter_mount = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "nsenter_mount");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int nsenter_uts = -1;
    lua_getfield(L, 1, "nsenter_uts");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_uts");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_uts");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "nsenter_uts");
            return lua_error(L);
        }
        nsenter_uts = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "nsenter_uts");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int nsenter_ipc = -1;
    lua_getfield(L, 1, "nsenter_ipc");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_ipc");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_ipc");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "nsenter_ipc");
            return lua_error(L);
        }
        nsenter_ipc = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "nsenter_ipc");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int nsenter_net = -1;
    lua_getfield(L, 1, "nsenter_net");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_net");
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, FILE_DESCRIPTOR_MT_INDEX)) {
            push(L, std::errc::invalid_argument, "arg", "nsenter_net");
            return lua_error(L);
        }
        lua_pop(L, 1);
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy, "arg", "nsenter_net");
            return lua_error(L);
        }
        nsenter_net = *handle;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "nsenter_net");
        return lua_error(L);
    }
    lua_pop(L, 1);

    int pipefd[2];
    if (pipe2(pipefd, O_DIRECT) == -1) {
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
    args.nsenter_user = nsenter_user;
    args.nsenter_mount = nsenter_mount;
    args.nsenter_uts = nsenter_uts;
    args.nsenter_ipc = nsenter_ipc;
    args.nsenter_net = nsenter_net;

    int clone_flags = CLONE_PIDFD;
    int pidfd = -1;
    int childpid = clone(system_spawn_child_main, clone_stack_address,
                         clone_flags, &args, &pidfd);
    if (childpid == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        if (pidfd != -1) {
            int res = kill(childpid, SIGKILL);
            boost::ignore_unused(res);
            siginfo_t info;
            res = waitid(P_PIDFD, pidfd, &info, WEXITED);
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
    new (p) subprocess{};

    p->reaper.emplace(
        vm_ctx.strand().context(), pidfd, childpid, signal_on_gcreaper);
    pidfd = -1;

    return 1;
}
EMILUA_GPERF_DECLS_END(spawn)

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

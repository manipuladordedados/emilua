// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <string_view>

#include <boost/asio/io_context.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/nowide/args.hpp>

#include <emilua/state.hpp>

#if BOOST_OS_LINUX
#include <boost/context/fixedsize_stack.hpp>
#endif // BOOST_OS_LINUX

#if BOOST_OS_UNIX
#include <emilua/actor.hpp>
#include <sys/wait.h>
#endif // BOOST_OS_UNIX

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

extern char** environ;

extern "C" {

std::optional<std::string_view>
BOOST_SYMBOL_EXPORT
__wrap_emilua_get_builtin_module(const std::filesystem::path& p)
{
    if (p == "/app/main.lua") {
        return "print('Hello World')\n";
    } else {
        return std::nullopt;
    }
}

} // extern "C"

int main(int argc, char *argv[], char *envp[])
{
#if BOOST_OS_UNIX
    {
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        for (int signo = 1 ; signo != NSIG ; ++signo) {
            sigaction(signo, /*act=*/&sa, /*oldact=*/NULL);
        }

        sigset_t set;
        sigfillset(&set);
        sigprocmask(SIG_UNBLOCK, &set, /*oldset=*/NULL);
    }
#endif // BOOST_OS_UNIX

#if BOOST_OS_LINUX
    boost::context::fixedsize_stack clone_stack_allocator;
    auto clone_stack = clone_stack_allocator.allocate();
    emilua::clone_stack_address = clone_stack.sp;
#endif // BOOST_OS_LINUX

#if BOOST_OS_UNIX
    emilua::app_context::environp = &environ;

    int ipc_actor_service_pipe[2];
    if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, ipc_actor_service_pipe) == -1) {
        ipc_actor_service_pipe[0] = -1;
        ipc_actor_service_pipe[1] = -1;
        perror("<4>Failed to start subprocess-based actor subsystem");
    }

    if (ipc_actor_service_pipe[0] != -1) {
        shutdown(ipc_actor_service_pipe[0], SHUT_WR);
        shutdown(ipc_actor_service_pipe[1], SHUT_RD);

        switch (fork()) {
        case -1: {
            perror("<4>Failed to start subprocess-based actor subsystem");
            close(ipc_actor_service_pipe[0]);
            close(ipc_actor_service_pipe[1]);
            ipc_actor_service_pipe[0] = -1;
            ipc_actor_service_pipe[1] = -1;
            break;
        }
        case 0:
            close(ipc_actor_service_pipe[1]);
            return emilua::app_context::ipc_actor_service_main(
                ipc_actor_service_pipe[0]);
        default: {
            close(ipc_actor_service_pipe[0]);
            ipc_actor_service_pipe[0] = -1;

            emilua::bzero_region args;

            // These regions are computed from this process so we try (as the
            // best of our efforts) to avoid loading them into extra
            // registers/stack of the other process.
            //
            // Ideally we'd control the whole chain of executed instructions up
            // to the point of calling explicit_bzero(), but that means writing
            // our own libc implementation and writing everything in
            // assembly. It's not really feasible to do the right thing here so
            // we just settle for a weaker compromise using portable C + glibc's
            // explicit_bzero().
            //
            // The alternative execve()-based approach provides stronger
            // isolation by flushing the whole address space, but comes with its
            // own limitations. By providing both approaches we can attack a
            // wider range of problems. IOW, there are no plans to remove this
            // code (the bzero() protection might not be a complete solution,
            // but it works usually well in practice).
            for (int i = 1 ; i < argc ; ++i) {
                std::string_view view = argv[i];
                args.s = const_cast<char*>(view.data());
                args.n = view.size();
                write(ipc_actor_service_pipe[1], &args, sizeof(args));
            }
            for (char** rawenv = envp ; *rawenv ; ++rawenv) {
                std::string_view env{*rawenv};
                args.s = const_cast<char*>(env.data());
                args.n = env.size();
                write(ipc_actor_service_pipe[1], &args, sizeof(args));
            }
            args.s = nullptr;
            args.n = 0;
            write(ipc_actor_service_pipe[1], &args, sizeof(args));
        }
        }
    }
#endif // BOOST_OS_UNIX

    boost::nowide::args args(argc, argv, envp);

    emilua::stdout_has_color = false;

    int main_ctx_concurrency_hint = BOOST_ASIO_CONCURRENCY_HINT_SAFE;
    emilua::ContextType main_context_type = emilua::ContextType::main;
    emilua::app_context appctx;

#if BOOST_OS_UNIX
    appctx.ipc_actor_service_sockfd = ipc_actor_service_pipe[1];
#endif // BOOST_OS_UNIX

    if (appctx.app_args.size() == 0) {
        appctx.app_args.reserve(2);
        appctx.app_args.emplace_back(argv[0]);
        appctx.app_args.emplace_back("");
    }

    appctx.emilua_path.emplace_back("/app", fs::path::generic_format);

    {
        asio::io_context ioctx{main_ctx_concurrency_hint};
        asio::make_service<emilua::properties_service>(
            ioctx, main_ctx_concurrency_hint);

        try {
            auto vm_ctx = emilua::make_vm(
                ioctx, appctx, main_context_type,
                fs::path{"/app/main.lua", fs::path::generic_format});
            appctx.master_vm = vm_ctx;
            vm_ctx->strand().post([vm_ctx]() {
                vm_ctx->fiber_resume(
                    vm_ctx->L(),
                    hana::make_set(
                        emilua::vm_context::options::skip_clear_interrupter));
            }, std::allocator<void>{});
        } catch (std::exception& e) {
            try {
                boost::nowide::cerr << "Error starting the lua VM: " <<
                    e.what() << std::endl;
            } catch (const std::ios_base::failure&) {}
            return 1;
        }

        ioctx.run();
    }

    {
        std::unique_lock<std::mutex> lk{appctx.extra_threads_count_mtx};
        while (appctx.extra_threads_count > 0)
            appctx.extra_threads_count_empty_cond.wait(lk);
    }

    return appctx.exit_code;
}

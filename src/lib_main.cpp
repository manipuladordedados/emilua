// Copyright (c) 2020, 2021, 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <string_view>
#include <charconv>
#include <optional>

#include <fmt/ostream.h>
#include <fmt/format.h>

#include <boost/preprocessor/stringize.hpp>
#include <boost/nowide/iostream.hpp>
#include <boost/nowide/args.hpp>
#include <boost/version.hpp>

#include <emilua/windows.hpp>
#include <emilua/state.hpp>

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/io_context.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/io_context.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

#if BOOST_OS_LINUX
#include <boost/context/fixedsize_stack.hpp>
#endif // BOOST_OS_LINUX

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/version.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

#if EMILUA_CONFIG_ENABLE_COLOR
#include <cstdlib>
#include <cstdio>

extern "C" {
#include <curses.h>
#include <term.h>
} // extern "C"
#endif // EMILUA_CONFIG_ENABLE_COLOR

#if BOOST_OS_UNIX
#include <emilua/actor.hpp>
#include <sys/eventfd.h>
#include <sys/wait.h>
#endif // BOOST_OS_UNIX

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua::main {

#if BOOST_OS_UNIX
static std::array<bool, 7> lowfds {
    false, false, false, false, false, false, false };
static int ipc_actor_service_sockfd = -1;
#endif // BOOST_OS_UNIX
static std::unordered_map<std::string_view, std::string_view> tmp_env;

// Useful to run an Emilua program inside Docker w/o any sort of mini-init
// supervisor. Also useful if you want to use Emilua to create your own init
// system.
//
// You might want to override this function to no-op if you're creating a binary
// to run from an initramfs image to exec() into systemD.
#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void depart_pid1()
{
#if BOOST_OS_UNIX
    if (getpid() == 1) {
        int evfd = eventfd(0, EFD_SEMAPHORE);
        if (evfd == -1)
            std::exit(1);
        auto atfork_parent = [&evfd]() -> std::optional<int> {
            if (eventfd_write(evfd, 1) == -1)
                return 1;

            return std::nullopt;
        };

        auto exit_code = app_context::handle_pid1(atfork_parent);
        if (exit_code)
            std::exit(*exit_code);

        eventfd_t evval;
        if (eventfd_read(evfd, &evval) == -1)
            std::exit(1);
        close(evfd);
    }
#endif // BOOST_OS_UNIX
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void parse_lowfds()
{
#if BOOST_OS_UNIX
    for (int fd = 3 ; fd <= 9 ; ++fd) {
        if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) {
            lowfds[fd - 3] = true;
        }
    }
#endif // BOOST_OS_UNIX
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void start_forker_service(int argc, char *argv[], char *envp[])
{
#if BOOST_OS_UNIX
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
        case 0: {
            close(ipc_actor_service_pipe[1]);
            int exit_code =  app_context::ipc_actor_service_main(
                ipc_actor_service_pipe[0]);
            std::exit(exit_code);
        }
        default: {
            close(ipc_actor_service_pipe[0]);
            ipc_actor_service_pipe[0] = -1;

            bzero_region args;

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

    ipc_actor_service_sockfd = ipc_actor_service_pipe[1];
#else // BOOST_OS_UNIX
    std::ignore = argc;
    std::ignore = argv;
    std::ignore = envp;
#endif // BOOST_OS_UNIX
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void register_eintr_rtsigno_handler()
{
#if BOOST_OS_UNIX
    if (EMILUA_CONFIG_EINTR_RTSIGNO != 0) {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(struct sigaction));

        // from SIG_UNBLOCK to SIG_BLOCK, execution MUST only use
        // async-signal-safe code
        sigemptyset(&sa.sa_mask);
        sigaddset(&sa.sa_mask, EMILUA_CONFIG_EINTR_RTSIGNO);
        sigprocmask(SIG_BLOCK, &sa.sa_mask, /*oldset=*/NULL);

        sa.sa_sigaction = longjmp_on_rtsigno;
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        sigaction(EMILUA_CONFIG_EINTR_RTSIGNO, /*act=*/&sa, /*oldact=*/NULL);
    }
#endif // BOOST_OS_UNIX
}


#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void set_locales()
{
    try {
        std::locale native_locale{""};
        std::locale::global(native_locale);
        boost::nowide::cin.imbue(native_locale);
        boost::nowide::cout.imbue(native_locale);
        boost::nowide::cerr.imbue(native_locale);
        boost::nowide::clog.imbue(native_locale);
    } catch (const std::exception& e) {
        try {
            fmt::print(
                boost::nowide::cerr,
                FMT_STRING("<4>Failed to set the native locale: `{}`\n"),
                e.what());
        } catch (const std::ios_base::failure&) {}
    }
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void parse_env(char *envp[])
{
    for (char** rawenv = envp ; *rawenv ; ++rawenv) {
        std::string_view env{*rawenv};
        auto i = env.find('=');
        auto key = env.substr(0, i);
        auto value = env.substr(i + 1);
        tmp_env.emplace(key, value);
    }

#if EMILUA_CONFIG_ENABLE_COLOR
    stdout_has_color = []() {
        if (auto it = tmp_env.find("EMILUA_COLORS") ; it != tmp_env.end()) {
            if (it->second == "1") {
                return true;
            } else if (it->second == "0") {
                return false;
            } else if (it->second.size() > 0) {
                try {
                    boost::nowide::cerr <<
                        "<4>Ignoring unrecognized value for EMILUA_COLORS\n";
                } catch (const std::ios_base::failure&) {}
            }
        }

        // Emilua runtime by itself will only ever dirt stderr
        if (!isatty(fileno(stderr)))
            return false;

        int ec = 0;
        if (setupterm(NULL, fileno(stderr), &ec) == ERR)
            return false;

        bool ret = tigetnum("colors") > 0;
        del_curterm(cur_term);
        return ret;
    }();
#else
    stdout_has_color = false;
#endif // EMILUA_CONFIG_ENABLE_COLOR

    if (auto it = tmp_env.find("EMILUA_LOG_LEVELS") ; it != tmp_env.end()) {
        std::string_view env = it->second;
        int level;
        auto res = std::from_chars(
            env.data(), env.data() + env.size(), level);
        if (res.ec == std::errc{})
            log_domain<default_log_domain>::log_level = level;
    }
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void fill_env(app_context& appctx)
{
    appctx.app_env = std::move(tmp_env);
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void fill_lowfds(app_context& appctx)
{
#if BOOST_OS_UNIX
    appctx.lowfds = lowfds;
#else // BOOST_OS_UNIX
    std::ignore = appctx;
#endif // BOOST_OS_UNIX
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void fill_forker_service_socket(app_context& appctx)
{
#if BOOST_OS_UNIX
    appctx.ipc_actor_service_sockfd = ipc_actor_service_sockfd;
#else // BOOST_OS_UNIX
    std::ignore = appctx;
#endif // BOOST_OS_UNIX
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void parse_args(int argc, char *argv[], app_context& appctx)
{
    appctx.app_args.resize(2);
    if (argc > 0)
        appctx.app_args[0] = argv[0];

    char** cur_arg = argv;
    while (*++cur_arg) {
        appctx.app_args.emplace_back(*cur_arg);
    }
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void fill_emilua_path(app_context& appctx)
{
    if (
        auto it = appctx.app_env.find("EMILUA_PATH") ;
        it != appctx.app_env.end()
    ) {
        for (std::string_view spec{it->second} ;;) {
            std::string_view::size_type sepidx = spec.find(
#if BOOST_OS_WINDOWS
                ';'
#else
                ':'
#endif
            );
            if (sepidx == std::string_view::npos) {
                appctx.emilua_path.emplace_back(
                    widen_on_windows(spec), fs::path::native_format);
                appctx.emilua_path.back().make_preferred();
                break;
            } else {
                appctx.emilua_path.emplace_back(
                    widen_on_windows(spec.substr(0, sepidx)),
                    fs::path::native_format);
                appctx.emilua_path.back().make_preferred();
                spec.remove_prefix(sepidx + 1);
            }
        }
    }

    appctx.emilua_path.emplace_back(
        widen_on_windows(EMILUA_CONFIG_LIBROOTDIR), fs::path::native_format);
    appctx.emilua_path.back().make_preferred();
    // TODO: Remove VERSION_MINOR from path components once emilua reaches
    // version 1.0.0 (versions that differ only in minor and patch numbers do
    // not break API).
    appctx.emilua_path.back() /=
        "emilua-" BOOST_PP_STRINGIZE(EMILUA_CONFIG_VERSION_MAJOR)
        "." BOOST_PP_STRINGIZE(EMILUA_CONFIG_VERSION_MINOR);
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void create_native_modules(
    const std::unique_lock<std::shared_mutex>& /*modules_cache_registry_wlock*/,
    app_context& /*appctx*/)
{}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void destroy_native_modules()
{}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
int main_ctx_concurrency_hint()
{
#if EMILUA_CONFIG_USE_STANDALONE_ASIO
    return ASIO_CONCURRENCY_HINT_SAFE;
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
    return BOOST_ASIO_CONCURRENCY_HINT_SAFE;
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void make_master_vm(app_context& appctx, asio::io_context& ioctx)
{}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
void run(asio::io_context& ioctx)
{
    ioctx.run();
}

#if defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
[[gnu::weak]]
#endif // defined(EMILUA_STATIC_BUILD) && !BOOST_OS_WINDOWS
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

    depart_pid1();

#if BOOST_OS_UNIX
    {
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;

        sigaction(SIGPIPE, /*act=*/&sa, /*oldact=*/NULL);
    }
#endif // BOOST_OS_UNIX

#if BOOST_OS_UNIX
    if (fcntl(0, F_GETFD) == -1 && errno == EBADF) {
        int p[2];
        if (pipe(p) == -1)
            std::abort();
        if (dup2(p[0], 0) == -1)
            std::abort();
        if (p[0] != 0)
            close(p[0]);
        if (p[1] != 0)
            close(p[1]);
    }

    for (int fd = 1 ; fd <= 2 ; ++fd) {
        if (fcntl(fd, F_GETFD) == -1 && errno == EBADF) {
            int p[2];
            if (pipe(p) == -1)
                std::abort();
            if (dup2(p[1], fd) == -1)
                std::abort();
            if (p[0] != fd)
                close(p[0]);
            if (p[1] != fd)
                close(p[1]);
        }
    }
#endif // BOOST_OS_UNIX

    parse_lowfds();

#if BOOST_OS_LINUX
    boost::context::fixedsize_stack clone_stack_allocator;
    auto clone_stack = clone_stack_allocator.allocate();
    clone_stack_address = clone_stack.sp;
#endif // BOOST_OS_LINUX

    start_forker_service(argc, argv, envp);
    register_eintr_rtsigno_handler();
    set_locales();

    std::optional<boost::nowide::args> args = std::nullopt;
    try {
        args.emplace(argc, argv, envp);
    } catch (const std::exception& e) {
        try {
            fmt::print(
                boost::nowide::cerr,
                FMT_STRING("Failed to read args/envp: `{}`\n"),
                e.what());
        } catch (const std::ios_base::failure&) {}
        return 2;
    }

    parse_env(envp);

    app_context appctx;
    fill_env(appctx);
    fill_lowfds(appctx);
    fill_forker_service_socket(appctx);
    parse_args(argc, argv, appctx);
    fill_emilua_path(appctx);

    {
        const std::unique_lock wlock{appctx.modules_cache_registry_mtx};
        create_native_modules(wlock, appctx);
    }

    {
#if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 2
        auto main_ctx_concurrency_hint_ = main_ctx_concurrency_hint();
        asio::io_context ioctx{main_ctx_concurrency_hint_};
        asio::make_service<properties_service>(
            ioctx, main_ctx_concurrency_hint_);
#elif EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 1
        asio::io_context ioctx{1};
        asio::make_service<properties_service>(ioctx, 1);
#elif EMILUA_CONFIG_THREAD_SUPPORT_LEVEL == 0
# if EMILUA_CONFIG_USE_STANDALONE_ASIO
        asio::io_context ioctx{ASIO_CONCURRENCY_HINT_UNSAFE};
# else // EMILUA_CONFIG_USE_STANDALONE_ASIO
        asio::io_context ioctx{BOOST_ASIO_CONCURRENCY_HINT_UNSAFE};
# endif // EMILUA_CONFIG_USE_STANDALONE_ASIO
        asio::make_service<properties_service>(ioctx, 1);
#else
# error Invalid thread support level
#endif

        try {
            make_master_vm(appctx, ioctx);
        } catch (std::exception& e) {
            try {
                boost::nowide::cerr << "Error starting the lua VM: " <<
                    e.what() << std::endl;
            } catch (const std::ios_base::failure&) {}
            return 1;
        }
        run(ioctx);
    }

    {
        std::unique_lock<std::mutex> lk{appctx.extra_threads_count_mtx};
        while (appctx.extra_threads_count > 0)
            appctx.extra_threads_count_empty_cond.wait(lk);
    }

    destroy_native_modules();

    return appctx.exit_code;
}

} // namespace emilua::main

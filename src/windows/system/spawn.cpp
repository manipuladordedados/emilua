/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>
#include <emilua/async_base.hpp>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/windows/object_handle.hpp>
#include <boost/asio/readable_pipe.hpp>
#include <boost/asio/writable_pipe.hpp>
#include <boost/asio/connect_pipe.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/filesystem.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
namespace fs = std::filesystem;

static char subprocess_mt_key;
static char subprocess_wait_key;

struct subprocess
{
    subprocess(asio::io_context& ctx, HANDLE hProcess, DWORD dwProcessId)
        : process{ctx, hProcess}
        , dwProcessId{dwProcessId}
        , status{std::in_place_type_t<void>{}}
    {}

    asio::windows::object_handle process;
    bool wait_in_progress = false;
    DWORD dwProcessId;
    result<DWORD, void> status;
};

template<class F>
static void env_path_for_each(std::string_view value, F&& f)
{
    std::string_view::const_iterator semicolon = value.begin();
    std::string_view::const_iterator next_semicolon;
    do {
        next_semicolon = std::find(semicolon, value.end(), ';');
        f(value.substr(semicolon - value.begin(), next_semicolon - semicolon));
        semicolon = next_semicolon;

        if (next_semicolon != value.end())
            ++semicolon;
    } while (semicolon != value.end());
}
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

    if (!p->process.is_open()) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    if (p->wait_in_progress) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    p->wait_in_progress = true;

    p->process.async_wait(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,p](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                p->wait_in_progress = false;

                DWORD status;
                GetExitCodeProcess(p->process.native_handle(), &status);
                p->status = status;

                boost::system::error_code ignored_ec;
                p->process.close(ignored_ec);

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
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

    if (!p->process.is_open()) {
        push(L, std::errc::no_such_process);
        return lua_error(L);
    }

    UINT uExitCode = lua_tointeger(L, 2);

    // TerminateProcess() and kill() are different beasts. kill() sends an UNIX
    // signal. TerminateProcess() doesn't send an UNIX
    // signal. TerminateProcess() directly sets the process' exit code. For
    // consistency with other platforms supported by Emilua, we use 128 + signo
    // as the exit code. User is not calling TerminateProcess(). User is calling
    // kill() so kill() semantics are expected. Also subprocess is a direct
    // descendent of the calling process so we have more freedom to define its
    // conventions.
    uExitCode += 128;

    if (!TerminateProcess(p->process.native_handle(), uExitCode)) {
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

    lua_pushinteger(L, p->status.value());
    return 1;
}

inline int subprocess_exit_signal(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->status) {
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    lua_pushnil(L);
    return 1;
}

inline int subprocess_pid(lua_State* L)
{
    auto p = static_cast<subprocess*>(lua_touserdata(L, 1));
    if (!p->process.is_open()) {
        push(L, std::errc::no_child_process);
        return lua_error(L);
    }

    lua_pushinteger(L, p->dwProcessId);
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

inline bool is_cmdexe_metacharacter(wchar_t c)
{
    // L'"' is a metacharacter as well, but we handle it separately so it
    // doesn't need to be included in this list.
    //
    // This list probably contains more characters than are really needed.
    return c == L'(' || c == L')' || c == L'%' || c == L'!' || c == L'^' ||
        c == L'<' || c == L'>' || c == L'&' || c == L'|' || c == L'@' ||
        c == L'[' || c == L']' || c == L'{' || c == L'}';
}

static
void quote_and_append_argv(std::wstring& command_line, const std::wstring& arg,
                           bool cmdexe_quoting)
{
    if (
        !arg.empty() && !cmdexe_quoting &&
        arg.find_first_of(L" \t\v\n\"") == std::wstring::npos
    ) {
        command_line.append(arg);
    } else {
        if (cmdexe_quoting) {
            command_line.append(L"^\"");
        } else {
            command_line.push_back(L'"');
        }

        for (auto it = arg.begin() ;; ++it) {
            std::size_t backslash_count = 0;

            while (it != arg.end() && *it == L'\\') {
                ++it;
                ++backslash_count;
            }

            if (it == arg.end()) {
                command_line.append(backslash_count * 2, L'\\');
                break;
            } else if (*it == L'"') {
                command_line.append(backslash_count * 2 + 1, L'\\');
                if (cmdexe_quoting) {
                    command_line.append(L"^\"");
                } else {
                    command_line.push_back(*it);
                }
            } else {
                command_line.append(backslash_count, L'\\');
                if (cmdexe_quoting && is_cmdexe_metacharacter(*it)) {
                    command_line.push_back(L'^');
                }
                command_line.push_back(*it);
            }
        }

        if (cmdexe_quoting) {
            command_line.append(L"^\"");
        } else {
            command_line.push_back(L'"');
        }
    }
}

int system_spawn(lua_State* L)
{
    using boost::algorithm::iequals;

    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    const int FILE_DESCRIPTOR_MT_INDEX = lua_gettop(L);
    auto& vm_ctx = get_vm_context(L);

    std::filesystem::path program;
    std::wstring command_line;
    bool use_comspec;

    lua_getfield(L, 1, "program");
    switch (lua_type(L, -1)) {
    case LUA_TSTRING: {
        boost::container::small_vector<const wchar_t*, 4> pathext;
        auto append_extension = [&pathext](std::string_view ext) {
            if (iequals(ext, ".cmd", std::locale::classic())) {
                pathext.emplace_back(L".cmd");
            } else if (iequals(ext, ".bat", std::locale::classic())) {
                pathext.emplace_back(L".bat");
            } else if (iequals(ext, ".exe", std::locale::classic())) {
                pathext.emplace_back(L".exe");
            } else if (iequals(ext, ".com", std::locale::classic())) {
                pathext.emplace_back(L".com");
            }
        };
        if (
            auto it = vm_ctx.appctx.app_env.find("PATHEXT") ;
            it != vm_ctx.appctx.app_env.end()
        ) {
            env_path_for_each(it->second, append_extension);
        } else {
            for (const auto& env : vm_ctx.appctx.app_env) {
                if (iequals(env.first, "PATHEXT", std::locale::classic())) {
                    env_path_for_each(env.second, append_extension);
                    break;
                }
            }
            if (pathext.empty()) {
                pathext.emplace_back(L".com");
                pathext.emplace_back(L".exe");
                pathext.emplace_back(L".bat");
                pathext.emplace_back(L".cmd");
            }
        }
        wchar_t path[MAX_PATH];
        for (const auto& ext : pathext) {
            if (SearchPathW(
                nullptr, nowide::widen(tostringview(L)).c_str(), ext, MAX_PATH,
                path, nullptr
            )) {
                if (
                    ext == std::wstring_view{L".cmd"} ||
                    ext == std::wstring_view{L".bat"}
                ) {
                    if (
                        auto it = vm_ctx.appctx.app_env.find("ComSpec") ;
                        it != vm_ctx.appctx.app_env.end()
                    ) {
                        program = fs::path{
                            nowide::widen(it->second), fs::path::native_format};
                        command_line.append(L"/C ");
                        quote_and_append_argv(command_line, path, true);
                    } else if (
                        auto it = vm_ctx.appctx.app_env.find("COMSPEC") ;
                        it != vm_ctx.appctx.app_env.end()
                    ) {
                        program = fs::path{
                            nowide::widen(it->second), fs::path::native_format};
                        command_line.append(L"/C ");
                        quote_and_append_argv(command_line, path, true);
                    } else {
                        for (const auto& env : vm_ctx.appctx.app_env) {
                            if (iequals(
                                env.first, "COMSPEC", std::locale::classic()
                            )) {
                                program = fs::path{
                                    nowide::widen(it->second),
                                    fs::path::native_format};
                                command_line.append(L"/C ");
                                quote_and_append_argv(command_line, path, true);
                                break;
                            }
                        }
                        if (program.empty()) {
                            push(L, std::errc::no_such_file_or_directory);
                            return lua_error(L);
                        }
                    }
                    use_comspec = true;
                } else {
                    program = path;
                    use_comspec = false;
                }
                break;
            }
        }
        if (program.empty()) {
            push(L, std::errc::no_such_file_or_directory);
            return lua_error(L);
        }
        break;
    }
    case LUA_TUSERDATA: {
        auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", "program");
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", "program");
            return lua_error(L);
        }
        lua_pop(L, 1);

        try {
            auto ext = path->extension().wstring();
            if (
                iequals(
                    ext, std::wstring_view{L".cmd"}, std::locale::classic()) ||
                iequals(ext, std::wstring_view{L".bat"}, std::locale::classic())
            ) {
                if (
                    auto it = vm_ctx.appctx.app_env.find("ComSpec") ;
                    it != vm_ctx.appctx.app_env.end()
                ) {
                    program = fs::path{
                        nowide::widen(it->second), fs::path::native_format};
                    command_line.append(L"/C ");
                    quote_and_append_argv(command_line, path->wstring(), true);
                } else if (
                    auto it = vm_ctx.appctx.app_env.find("COMSPEC") ;
                    it != vm_ctx.appctx.app_env.end()
                ) {
                    program = fs::path{
                        nowide::widen(it->second), fs::path::native_format};
                    command_line.append(L"/C ");
                    quote_and_append_argv(command_line, path->wstring(), true);
                } else {
                    for (const auto& env : vm_ctx.appctx.app_env) {
                        if (iequals(
                            env.first, "COMSPEC", std::locale::classic()
                        )) {
                            program = fs::path{
                                nowide::widen(it->second),
                                fs::path::native_format};
                            command_line.append(L"/C ");
                            quote_and_append_argv(
                                command_line, path->wstring(), true);
                            break;
                        }
                    }
                    if (program.empty()) {
                        push(L, std::errc::no_such_file_or_directory);
                        return lua_error(L);
                    }
                }
                use_comspec = true;
            } else {
                program = *path;
                use_comspec = false;
            }
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
        push(L, std::errc::invalid_argument, "arg", "program");
        return lua_error(L);
    }
    lua_pop(L, 1);

    lua_getfield(L, 1, "arguments");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        for (int i = (use_comspec ? 2 : 1) ;; ++i) {
            lua_rawgeti(L, -1, i);
            switch (lua_type(L, -1)) {
            case LUA_TNIL:
                lua_pop(L, 1);
                goto end_for;
            case LUA_TSTRING:
                if (i != 1)
                    command_line.push_back(L' ');

                quote_and_append_argv(
                    command_line, nowide::widen(tostringview(L)), use_comspec);
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

    std::vector<std::wstring> environment;
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

            auto v = nowide::widen(tostringview(L, -2));
            v.push_back(L'=');
            v.append(nowide::widen(tostringview(L, -1)));
            environment.insert(
                std::upper_bound(environment.begin(), environment.end(), v),
                v);

            lua_pop(L, 1);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "environment");
        return lua_error(L);
    }
    lua_pop(L, 1);
    std::wstring environmentb;
    std::size_t environment_size = 0;
    for (auto& e : environment) {
        environment_size += e.size() + 1;
    }
    environmentb.reserve(environment_size);
    for (auto& e : environment) {
        environmentb += e;
        environmentb.push_back(L'\0');
    }

    HANDLE proc_stdin = INVALID_HANDLE_VALUE;
    asio::readable_pipe stdin_rpipe{vm_ctx.strand().context()};
    asio::writable_pipe stdin_wpipe{vm_ctx.strand().context()};

    lua_getfield(L, 1, "stdin");
    switch (lua_type(L, -1)) {
    case LUA_TNIL: {
        boost::system::error_code ec;
        asio::connect_pipe(stdin_rpipe, stdin_wpipe, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        proc_stdin = stdin_rpipe.native_handle();
        SetHandleInformation(
            proc_stdin, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        break;
    }
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stdin");
            return lua_error(L);
        }
        proc_stdin = GetStdHandle(STD_INPUT_HANDLE);
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

    HANDLE proc_stdout = INVALID_HANDLE_VALUE;
    asio::readable_pipe stdout_rpipe{vm_ctx.strand().context()};
    asio::writable_pipe stdout_wpipe{vm_ctx.strand().context()};

    lua_getfield(L, 1, "stdout");
    switch (lua_type(L, -1)) {
    case LUA_TNIL: {
        boost::system::error_code ec;
        asio::connect_pipe(stdout_rpipe, stdout_wpipe, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        proc_stdout = stdout_wpipe.native_handle();
        SetHandleInformation(
            proc_stdout, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        break;
    }
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stdout");
            return lua_error(L);
        }
        proc_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
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

    HANDLE proc_stderr = INVALID_HANDLE_VALUE;
    asio::readable_pipe stderr_rpipe{vm_ctx.strand().context()};
    asio::writable_pipe stderr_wpipe{vm_ctx.strand().context()};

    lua_getfield(L, 1, "stderr");
    switch (lua_type(L, -1)) {
    case LUA_TNIL: {
        boost::system::error_code ec;
        asio::connect_pipe(stderr_rpipe, stderr_wpipe, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        proc_stderr = stderr_wpipe.native_handle();
        SetHandleInformation(
            proc_stderr, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        break;
    }
    case LUA_TSTRING:
        if (tostringview(L) != "share") {
            push(L, std::errc::invalid_argument, "arg", "stderr");
            return lua_error(L);
        }
        proc_stderr = GetStdHandle(STD_ERROR_HANDLE);
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

    bool create_new_process_group = false;
    lua_getfield(L, 1, "process_group");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TNUMBER:
        if (lua_tointeger(L, -1) != 0) {
            push(L, std::errc::not_supported, "arg", "process_group");
            return lua_error(L);
        }
        create_new_process_group = true;
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", "process_group");
        return lua_error(L);
    }
    lua_pop(L, 1);

    std::filesystem::path working_directory;
    lua_getfield(L, 1, "working_directory");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        break;
    case LUA_TUSERDATA: {
        auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, -1));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", "working_directory");
            return lua_error(L);
        }
        lua_pop(L, 1);

        try {
            working_directory = *path;
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

    DWORD dwFlags = STARTF_USESTDHANDLES;

    WORD wShowWindow;
    lua_getfield(L, 1, "show_window");
    switch (lua_type(L, -1)) {
    case LUA_TNIL:
        wShowWindow = 0;
        break;
    case LUA_TSTRING: {
        auto value = tostringview(L);
        auto value2 = EMILUA_GPERF_BEGIN(value)
            EMILUA_GPERF_PARAM(WORD action)
            EMILUA_GPERF_PAIR("hide", SW_HIDE)
            EMILUA_GPERF_PAIR("shownormal", SW_SHOWNORMAL)
            EMILUA_GPERF_PAIR("normal", SW_NORMAL)
            EMILUA_GPERF_PAIR("showminimized", SW_SHOWMINIMIZED)
            EMILUA_GPERF_PAIR("showmaximized", SW_SHOWMAXIMIZED)
            EMILUA_GPERF_PAIR("maximize", SW_MAXIMIZE)
            EMILUA_GPERF_PAIR("shownoactivate", SW_SHOWNOACTIVATE)
            EMILUA_GPERF_PAIR("show", SW_SHOW)
            EMILUA_GPERF_PAIR("minimize", SW_MINIMIZE)
            EMILUA_GPERF_PAIR("showminnoactive", SW_SHOWMINNOACTIVE)
            EMILUA_GPERF_PAIR("showna", SW_SHOWNA)
            EMILUA_GPERF_PAIR("restore", SW_RESTORE)
            EMILUA_GPERF_PAIR("forceminimize", SW_FORCEMINIMIZE)
        EMILUA_GPERF_END(value);
        if (!value2) {
            push(L, std::errc::invalid_argument, "arg", "show_window");
            return lua_error(L);
        }
        dwFlags |= STARTF_USESHOWWINDOW;
        wShowWindow = *value2;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", "show_window");
        return lua_error(L);
    }
    lua_pop(L, 1);

    STARTUPINFOEXW startup_info{{
        sizeof(STARTUPINFOEXW), nullptr, nullptr, nullptr,
        0, 0, 0, 0, 0, 0, 0, dwFlags, wShowWindow, 0, nullptr,
        proc_stdin, proc_stdout, proc_stderr
    }, nullptr};
    BOOST_SCOPE_EXIT_ALL(&) { if (startup_info.lpAttributeList) {
        HeapFree(GetProcessHeap(), 0, startup_info.lpAttributeList);
    } };

    {
        SIZE_T size = 0;

        if (!InitializeProcThreadAttributeList(NULL, 1, 0, &size)) {
            DWORD last_error = GetLastError();
            if (last_error != ERROR_INSUFFICIENT_BUFFER) {
                boost::system::error_code ec(
                    last_error, asio::error::get_system_category());
                push(L, static_cast<std::error_code>(ec));
                return lua_error(L);
            }
        }

        startup_info.lpAttributeList = reinterpret_cast<
            LPPROC_THREAD_ATTRIBUTE_LIST>(HeapAlloc(GetProcessHeap(), 0, size));
        if (!startup_info.lpAttributeList) {
            push(L, std::errc::not_enough_memory);
            return lua_error(L);
        }

        if (!InitializeProcThreadAttributeList(
            startup_info.lpAttributeList, 1, 0, &size
        )) {
            DWORD last_error = GetLastError();
            boost::system::error_code ec(
                last_error, asio::error::get_system_category());
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        DeleteProcThreadAttributeList(startup_info.lpAttributeList);
    };

    HANDLE handles_to_inherit[3] = { proc_stdin, proc_stdout, proc_stderr };

    if (!UpdateProcThreadAttribute(
        startup_info.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        handles_to_inherit, 3 * sizeof(HANDLE), NULL, NULL
    )) {
        DWORD last_error = GetLastError();
        boost::system::error_code ec(
            last_error, asio::error::get_system_category());
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }

    PROCESS_INFORMATION pi{nullptr, nullptr, 0, 0};
    BOOST_SCOPE_EXIT_ALL(&) {
        if (pi.hProcess != INVALID_HANDLE_VALUE) { CloseHandle(pi.hProcess); }
    };

    DWORD dwCreationFlags{
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT};

    if (start_new_session) {
        dwCreationFlags |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
    }

    if (create_new_process_group) {
        dwCreationFlags |= CREATE_NEW_PROCESS_GROUP;
    }

    BOOL ok = ::CreateProcessW(
        program.c_str(),
        command_line.data(),
        /*lpProcessAttributes=*/nullptr,
        /*lpThreadAttributes=*/nullptr,
        /*bInheritHandles=*/TRUE,
        dwCreationFlags,
        environmentb.data(),
        working_directory.empty() ? nullptr : working_directory.c_str(),
        &startup_info.StartupInfo,
        &pi);
    if (!ok) {
        DWORD last_error = GetLastError();
        boost::system::error_code ec(
            last_error, asio::error::get_system_category());
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }

    CloseHandle(pi.hThread);

    auto p = static_cast<subprocess*>(lua_newuserdata(L, sizeof(subprocess)));
    rawgetp(L, LUA_REGISTRYINDEX, &subprocess_mt_key);
    setmetatable(L, -2);
    new (p) subprocess{vm_ctx.strand().context(), pi.hProcess, pi.dwProcessId};
    pi.hProcess = INVALID_HANDLE_VALUE;

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

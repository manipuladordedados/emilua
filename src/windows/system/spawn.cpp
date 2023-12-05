/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/core.hpp>
#include <emilua/async_base.hpp>

#include <boost/asio/windows/object_handle.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/filesystem.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(system)
EMILUA_GPERF_NAMESPACE(emilua)
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

static
void quote_and_append_argv(std::wstring& command_line, const std::wstring& arg)
{
    if (!arg.empty() && arg.find_first_of(L" \t\v\n\"") == std::wstring::npos) {
        command_line.append(arg);
    } else {
        command_line.push_back(L'"');

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
                command_line.push_back(*it);
            } else {
                command_line.append(backslash_count, L'\\');
                command_line.push_back(*it);
            }
        }

        command_line.push_back(L'"');
    }
}

int system_spawn(lua_State* L)
{
    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TTABLE);
    auto& vm_ctx = get_vm_context(L);

    std::filesystem::path program;
    lua_getfield(L, 1, "program");
    switch (lua_type(L, -1)) {
    case LUA_TSTRING: {
        wchar_t path[MAX_PATH];
        if (!SearchPathW(
            nullptr, nowide::widen(tostringview(L)).c_str(), L".exe", MAX_PATH,
            path, nullptr
        )) {
            push(L, std::errc::no_such_file_or_directory);
            return lua_error(L);
        }
        program = path;
        break;
    }
    case LUA_TUSERDATA: {
        auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, -1));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", "program");
            return lua_error(L);
        }
        lua_pop(L, 1);

        try {
            program = *path;
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

    std::wstring command_line;
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
                if (i != 1)
                    command_line.push_back(L' ');

                quote_and_append_argv(
                    command_line, nowide::widen(tostringview(L)));
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

    STARTUPINFOW startup_info{
        sizeof(STARTUPINFOEXW), nullptr, nullptr, nullptr,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr,
        INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE
    };

    PROCESS_INFORMATION pi{nullptr, nullptr, 0, 0};
    BOOST_SCOPE_EXIT_ALL(&) {
        if (pi.hProcess != INVALID_HANDLE_VALUE) { CloseHandle(pi.hProcess); }
    };

    BOOL ok = ::CreateProcessW(
        program.c_str(),
        command_line.data(),
        /*lpProcessAttributes=*/nullptr,
        /*lpThreadAttributes=*/nullptr,
        /*bInheritHandles=*/FALSE,
        /*dwCreationFlags=*/0,
        environmentb.data(),
        working_directory.empty() ? nullptr : working_directory.c_str(),
        &startup_info,
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

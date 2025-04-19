// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/native_module.hpp>
#include <emilua/state.hpp>

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO


static int exit_code = 1;

struct native_module : public emilua::native_module
{
    ~native_module()
    {
        if (imported) {
            exit_code = 0;
        }
    }

    std::error_code init_lua_module(
        std::shared_lock<std::shared_mutex>&, emilua::vm_context& /*vm_ctx*/,
        lua_State* L) override
    {
        imported = true;

        lua_pushnil(L);
        return {};
    }

    bool imported = false;
};

std::optional<native_module> foobar333;

namespace emilua {

#if BOOST_OS_WINDOWS
extern
std::optional<std::string_view>
(*get_builtin_module)(const std::filesystem::path& p);
static
std::optional<std::string_view>
get_builtin_module2(const std::filesystem::path& p)
#else // BOOST_OS_WINDOWS
std::optional<std::string_view>
get_builtin_module(const std::filesystem::path& p)
#endif // BOOST_OS_WINDOWS
{
    fs::path target{"/NUL/app/main.lua", fs::path::generic_format};
    if (p.root_directory() / p.relative_path() == target) {
        return "require('foobar333')\n"
            "print('Hello World')\n";
    } else {
        return std::nullopt;
    }
}

#if BOOST_OS_WINDOWS
extern
std::optional<std::reference_wrapper<emilua::native_module>>
(*get_builtin_native_module)(std::string_view id);
static
std::optional<std::reference_wrapper<emilua::native_module>>
get_builtin_native_module2(std::string_view id)
#else // BOOST_OS_WINDOWS
std::optional<std::reference_wrapper<emilua::native_module>>
get_builtin_native_module(std::string_view id)
#endif // BOOST_OS_WINDOWS
{
    if (id == "foobar333") {
        return std::ref(static_cast<emilua::native_module&>(*foobar333));
    } else {
        return std::nullopt;
    }
}

#if BOOST_OS_WINDOWS
extern
void (*create_native_modules)(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx);
static
void create_native_modules2(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx)
#else // BOOST_OS_WINDOWS
void create_native_modules(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx)
#endif // BOOST_OS_WINDOWS
{
    foobar333.emplace();
    foobar333->init_appctx(modules_cache_registry_wlock, appctx);
}

#if BOOST_OS_WINDOWS
extern void (*destroy_native_modules)();
static void destroy_native_modules2()
#else // BOOST_OS_WINDOWS
void destroy_native_modules()
#endif // BOOST_OS_WINDOWS
{
    foobar333.reset();
}

namespace main {

#if BOOST_OS_WINDOWS
extern int (*main)(int argc, char *argv[], char *envp[]);
#else // BOOST_OS_WINDOWS
int main(int argc, char *argv[], char *envp[]);
#endif // BOOST_OS_WINDOWS

#if BOOST_OS_WINDOWS
extern void (*make_master_vm)(
    app_context& appctx, std::shared_ptr<asio::io_context> ioctx);
static void make_master_vm2(
    app_context& appctx, std::shared_ptr<asio::io_context> ioctx)
#else // BOOST_OS_WINDOWS
void make_master_vm(
    app_context& appctx, std::shared_ptr<asio::io_context> ioctx)
#endif // BOOST_OS_WINDOWS
{
    auto vm_ctx = make_vm(
        appctx, *ioctx, ContextType::main,
        fs::path{"/NUL/app/main.lua", fs::path::generic_format});
#if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 2
    vm_ctx->ioctxref = ioctx;
#endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL >= 2
    appctx.master_vm = vm_ctx;
    vm_ctx->strand().post([vm_ctx]() {
        vm_ctx->fiber_resume(
            vm_ctx->L(),
            hana::make_set(vm_context::options::skip_clear_interrupter));
    }, std::allocator<void>{});
}

} // namespace main

} // namespace emilua

int main(int argc, char *argv[], char *envp[])
{
#if BOOST_OS_WINDOWS
    emilua::get_builtin_module = emilua::get_builtin_module2;
    emilua::get_builtin_native_module = emilua::get_builtin_native_module2;
    emilua::create_native_modules = emilua::create_native_modules2;
    emilua::destroy_native_modules = emilua::destroy_native_modules2;
    emilua::main::make_master_vm = emilua::main::make_master_vm2;
#endif // BOOST_OS_WINDOWS
    std::ignore = emilua::main::main(argc, argv, envp);
    return exit_code;
}

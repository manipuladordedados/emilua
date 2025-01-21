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

std::optional<std::string_view>
get_builtin_module(const std::filesystem::path& p)
{
    if (p == "/app/main.lua") {
        return "require('foobar333')\n"
            "print('Hello World')\n";
    } else {
        return std::nullopt;
    }
}

std::optional<std::reference_wrapper<emilua::native_module>>
get_builtin_native_module(std::string_view id)
{
    if (id == "foobar333") {
        return std::ref(static_cast<emilua::native_module&>(*foobar333));
    } else {
        return std::nullopt;
    }
}

void create_native_modules(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx)
{
    foobar333.emplace();
    foobar333->init_appctx(modules_cache_registry_wlock, appctx);
}

void destroy_native_modules()
{
    foobar333.reset();
}

namespace main {

int main(int argc, char *argv[], char *envp[]);

void make_master_vm(app_context& appctx, asio::io_context& ioctx)
{
    auto vm_ctx = make_vm(
        ioctx, appctx, ContextType::main,
        fs::path{"/app/main.lua", fs::path::generic_format});
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
    std::ignore = emilua::main::main(argc, argv, envp);
    return exit_code;
}

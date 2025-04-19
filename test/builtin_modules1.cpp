// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/state.hpp>

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

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
        return "print('Hello World')\n";
    } else {
        return std::nullopt;
    }
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
    emilua::main::make_master_vm = emilua::main::make_master_vm2;
#endif // BOOST_OS_WINDOWS
    return emilua::main::main(argc, argv, envp);
}

// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/state.hpp>

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua {

std::optional<std::string_view>
get_builtin_module(const std::filesystem::path& p)
{
    if (p == "/app/main.lua") {
        return "print('Hello World')\n";
    } else {
        return std::nullopt;
    }
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
    return emilua::main::main(argc, argv, envp);
}

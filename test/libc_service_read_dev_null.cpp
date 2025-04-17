// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/native_module.hpp>
#include <emilua/state.hpp>

#include <iostream>
#include <fstream>

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

struct : public emilua::native_module
{
    std::error_code init_lua_module(
        std::shared_lock<std::shared_mutex>&, emilua::vm_context& /*vm_ctx*/,
        lua_State* L) override
    {
        lua_pushnil(L);

        std::ifstream in{"/dev/null", std::ios::in | std::ios::binary};
        in.imbue(std::locale::classic());
        in.exceptions(std::ios_base::failbit);
        std::string line;
        try {
            std::getline(in, line);
        } catch (const std::exception& e) {
            std::cout << "not ok 1 - getline: " << e.what() << std::endl;
            return {};
        }

        std::cout << line << std::endl;
        return {};
    }
} foobar333;

namespace emilua {

std::optional<std::string_view>
get_builtin_module(const std::filesystem::path& p)
{
    if (p == "/app/main.lua") {
        return R"lua(
local libc_service = require 'libc_service'
local stream = require 'stream'
local system = require 'system'
local pipe = require 'pipe'
local fs = require 'filesystem'

stream.write_all(system.out, '1..1\n')

local master, slave = libc_service.new()

slave.open = [[
local real_open, path, flag, mode = ...
local res, errno, fd = real_open(path, flag, mode)
if fd then
    return fd
else
    return res, errno
end
]]

local pi, po = pipe.pair()
po = po:release()

local source_tree_cache = {}
source_tree_cache['a.lua'] = "require('foobar333')\n"

local guest = spawn_vm{
    module = fs.path.new('/a.lua'),
    subprocess = {
        source_tree_cache = source_tree_cache,
        libc_service = slave,
        stdout = 'share',
        stderr = 'share',
    }
}
guest:send(po)
po:close()

spawn(function() pcall(function()
    while true do
        master:receive()
        if master.function_ ~= 'open' then
            master:use_slave_credentials()
            goto continue
        end
        local p, f, m = master:arguments()
        if p ~= fs.path.new('/dev/null') then
            master:use_slave_credentials()
            goto continue
        end

        local pi, po = pipe.pair()
        pi = pi:release()
        spawn(function()
            stream.write_all(po, 'ok 1\n')
            po:close()
        end):detach()
        master:send_with_fds(-2, {pi})
        ::continue::
    end
end) end):detach()

pcall(function() pi:read_some(byte_span.new(1)) end)
)lua";
    } else {
        return std::nullopt;
    }
}

std::optional<std::reference_wrapper<emilua::native_module>>
get_builtin_native_module(std::string_view id)
{
    if (id == "foobar333") {
        return std::ref(static_cast<emilua::native_module&>(foobar333));
    } else {
        return std::nullopt;
    }
}

namespace main {

int main(int argc, char *argv[], char *envp[]);

void make_master_vm(app_context& appctx, asio::io_context& ioctx)
{
    auto vm_ctx = make_vm(
        appctx, ioctx, ContextType::main,
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

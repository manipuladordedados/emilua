// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/native_module.hpp>
#include <emilua/filesystem.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/state.hpp>

#include <fstream>

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

static struct bio : public emilua::native_module
{
    std::error_code init_lua_module(
        std::shared_lock<std::shared_mutex>&, emilua::vm_context& /*vm_ctx*/,
        lua_State* L) override;
} plugin;

namespace emilua {

#if BOOST_OS_WINDOWS
extern std::optional<std::string_view> (*get_builtin_module)(const fs::path& p);
static std::optional<std::string_view> get_builtin_module2(const fs::path& p)
#else // BOOST_OS_WINDOWS
std::optional<std::string_view> get_builtin_module(const fs::path& p)
#endif // BOOST_OS_WINDOWS
{
    fs::path target{"/dev/null/NUL/app/init.lua", fs::path::generic_format};
    if (p.root_directory() / p.relative_path() == target) {
        return R"lua(
local stream = require 'stream'
local system = require 'system'
local pipe = require 'pipe'
local bio = require 'bio'
local fs = require 'filesystem'

local input
do
    local arguments = system.arguments
    table.remove(arguments, 2)
    table.remove(arguments, 1)
    if #arguments ~= 1 then
        stream.write_all(system.err, byte_span.append(
            'Usage:\n', '\n', 'genmain directory/containing/init.lua\n'))
        system.exit(1)
    end
    input = fs.path.new(arguments[1]):lexically_normal()
    if not fs.is_directory(input) then
        stream.write_all(system.err, format(
            '{:?} is not a directory\n', tostring(input)))
        system.exit(1)
    end
end

local source_tree = {}

for entry in fs.recursive_directory_iterator(input) do
    local k = entry.path
    if entry.status.type ~= 'regular' or k.extension ~= '.lua' then
        goto continue
    end
    local v = bio.read_file(k)
    if not v then
        v = ''
    end

    k = fs.path.from_generic('/dev/null/NUL/app') / k:lexically_relative(input)
    source_tree[k:to_generic()] = format('{:?}', tostring(v))

    ::continue::
end

stream.write_all(system.out, byte_span.append(
    '#include <cstddef>\n' ..
    '#include <cstring>\n' ..
    '#include <emilua/state.hpp>\n' ..
    'using std::size_t;\n' ..
    'using std::strcmp;\n' ..
    'namespace hana = boost::hana;\n' ..
    'namespace fs = std::filesystem;\n' ..
    'namespace {\n'
))

do
    local pi, po = pipe.pair()
    pi = pi:release()
    local p = system.spawn{
        program = 'gperf',
        arguments = {
            'gperf', '--language=C++', '--enum', '--readonly-tables',
            '--struct-type', '--initializer-suffix=, {}',
        },
        environment = system.environment,
        stdout = 'share',
        stderr = 'share',
        stdin = pi,
    }
    pi:close()
    stream.write_all(
        po, 'struct word { const char* name; std::string_view action; };\n%%\n')
    for k, v in pairs(source_tree) do
        stream.write_all(po, byte_span.append(
            k, ', ', v, '\n'
        ))
    end
    stream.write_all(po, '%%\n')
    po:close()
    p:wait()
    if p.exit_code ~= 0 then
        system.exit(1)
    end
end

stream.write_all(system.out, byte_span.append(
    '} // namespace\n' ..

    'namespace emilua {\n' ..
    [[
    std::optional<std::string_view> get_builtin_module(const fs::path& p)
    {
        auto k = (p.root_directory() / p.relative_path()).generic_string();
        auto v = Perfect_Hash::in_word_set(k.data(), k.size());
        if (!v) {
            return std::nullopt;
        }

        return v->action;
    }
    ]] ..
    '} // namespace emilua\n' ..

    'namespace emilua::main {\n' ..
    'int main(int argc, char *argv[], char *envp[]);\n' ..
    [[
    void make_master_vm(app_context& appctx, asio::io_context& ioctx)
    {
        auto vm_ctx = make_vm(
            ioctx, appctx, ContextType::main,
            fs::path{"/dev/null/NUL/app/init.lua", fs::path::generic_format});
        appctx.master_vm = vm_ctx;
        vm_ctx->strand().post([vm_ctx]() {
            vm_ctx->fiber_resume(
                vm_ctx->L(),
                hana::make_set(vm_context::options::skip_clear_interrupter));
        }, std::allocator<void>{});
    }
    ]] ..
    '} // namespace emilua::main\n' ..

    'int main(int argc, char *argv[], char *envp[]) {\n' ..
    '    return emilua::main::main(argc, argv, envp);\n' ..
    '}\n'
))
            )lua";
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
    if (id == "bio") {
        return std::ref(static_cast<emilua::native_module&>(plugin));
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
extern void (*make_master_vm)(app_context& appctx, asio::io_context& ioctx);
static void make_master_vm2(app_context& appctx, asio::io_context& ioctx)
#else // BOOST_OS_WINDOWS
void make_master_vm(app_context& appctx, asio::io_context& ioctx)
#endif // BOOST_OS_WINDOWS
{
    auto vm_ctx = make_vm(
        ioctx, appctx, ContextType::main,
        fs::path{"/dev/null/NUL/app/init.lua", fs::path::generic_format});
    appctx.master_vm = vm_ctx;
    vm_ctx->strand().post([vm_ctx]() {
        vm_ctx->fiber_resume(
            vm_ctx->L(),
            hana::make_set(vm_context::options::skip_clear_interrupter));
    }, std::allocator<void>{});
}

} // namespace main

} // namespace emilua

static int read_file(lua_State* L)
{
    using emilua::push;
    using emilua::rawgetp;
    using emilua::setmetatable;
    using emilua::byte_span_handle;

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &emilua::filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::ifstream file{*path, std::ios::in | std::ios::binary};
    file.imbue(std::locale::classic()); //< paranoia

    std::streampos sz;
    file.seekg(0, std::ios::end);
    sz = file.tellg();
    file.seekg(0, std::ios::beg);

    if (file.rdstate() != std::ios_base::goodbit) {
        push(L, std::io_errc::stream);
        return lua_error(L);
    }

    if (sz == 0) {
        lua_pushnil(L);
        return 1;
    }

    auto contents = std::make_shared_for_overwrite<unsigned char[]>(sz);
    file.read(reinterpret_cast<char*>(contents.get()), sz);

    if (file.rdstate() != std::ios_base::goodbit) {
        push(L, std::io_errc::stream);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(
        lua_newuserdata(L, sizeof(byte_span_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &emilua::byte_span_mt_key);
    setmetatable(L, -2);
    new (bs) byte_span_handle{std::move(contents), sz, sz};
    return 1;
}

std::error_code bio::init_lua_module(
    std::shared_lock<std::shared_mutex>&, emilua::vm_context& /*vm_ctx*/,
    lua_State* L)
{
    lua_newtable(L);

    lua_pushliteral(L, "read_file");
    lua_pushcfunction(L, read_file);
    lua_rawset(L, -3);

    return {};
}

int main(int argc, char *argv[], char *envp[])
{
#if BOOST_OS_WINDOWS
    emilua::get_builtin_module = emilua::get_builtin_module2;
    emilua::get_builtin_native_module = emilua::get_builtin_native_module2;
    emilua::main::make_master_vm = emilua::main::make_master_vm2;
#endif // BOOST_OS_WINDOWS
    return emilua::main::main(argc, argv, envp);
}

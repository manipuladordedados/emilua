// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/state.hpp>

namespace hana = boost::hana;
namespace fs = std::filesystem;

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua {

#if BOOST_OS_WINDOWS
extern std::optional<std::string_view> (*get_builtin_module)(const fs::path& p);
static std::optional<std::string_view> get_builtin_module2(const fs::path& p)
#else // BOOST_OS_WINDOWS
std::optional<std::string_view> get_builtin_module(const fs::path& p)
#endif // BOOST_OS_WINDOWS
{
    fs::path target{"/dev/null/app/init.lua", fs::path::generic_format};
    if (p == fs::absolute(target)) {
        return R"lua(
local stream = require 'stream'
local system = require 'system'
local pipe = require 'pipe'
local fs = require 'filesystem'

local input = system.arguments
table.remove(input, 2)
table.remove(input, 1)

for k, v in ipairs(input) do
    local pi, po = pipe.pair()
    po = po:release()
    local p = system.spawn{
        program = 'pkg-config',
        arguments = {
            'pkg-config', '--variable=main_header', v,
        },
        environment = system.environment,
        stderr = 'share',
        stdout = po,
    }
    po:close()
    pi = stream.scanner.new{ stream = pi }
    local main_header = tostring(pi:get_line())
    p:wait()
    if p.exit_code ~= 0 then
        system.exit(1)
    end

    local pi, po = pipe.pair()
    po = po:release()
    local p = system.spawn{
        program = 'pkg-config',
        arguments = {
            'pkg-config', '--variable=main_type', v,
        },
        environment = system.environment,
        stderr = 'share',
        stdout = po,
    }
    po:close()
    pi = stream.scanner.new{ stream = pi }
    local main_type = tostring(pi:get_line())
    p:wait()
    if p.exit_code ~= 0 then
        system.exit(1)
    end

    local pi, po = pipe.pair()
    po = po:release()
    local p = system.spawn{
        program = 'pkg-config',
        arguments = {
            'pkg-config', '--variable=plugin_id', v,
        },
        environment = system.environment,
        stderr = 'share',
        stdout = po,
    }
    po:close()
    pi = stream.scanner.new{ stream = pi }
    local plugin_id = tostring(pi:get_line())
    p:wait()
    if p.exit_code ~= 0 then
        system.exit(1)
    end

    input[k] = { header = main_header, type = main_type, id = plugin_id }
end

for k, v in ipairs(input) do
    stream.write_all(system.out, byte_span.append(
        '#include ' ..
        v.header ..
        '\n'
    ))
end

stream.write_all(system.out, byte_span.append(
    '#include <cstddef>\n' ..
    '#include <cstring>\n' ..
    '#include <array>\n' ..
    '#include <emilua/native_module.hpp>\n' ..
    'using std::size_t;\n' ..
    'using std::strcmp;\n' ..
    'namespace {\n' ..
    'std::array<emilua::native_module*, ' ..
    tostring(#input) ..
    '> modules_indexes;\n'
))

for k, v in ipairs(input) do
    stream.write_all(
        system.out, format('std::optional<{}> plugin{};\n', v.type, k - 1))
end

do
    local pi, po = pipe.pair()
    pi = pi:release()
    local p = system.spawn{
        program = 'gperf',
        arguments = {
            'gperf', '--language=C++', '--enum', '--readonly-tables',
            '--struct-type', '--initializer-suffix=, 0',
        },
        environment = system.environment,
        stdout = 'share',
        stderr = 'share',
        stdin = pi,
    }
    pi:close()
    stream.write_all(
        po, 'struct word { const char* name; int action; };\n%%\n')
    for k, v in ipairs(input) do
        stream.write_all(po, byte_span.append(
            v.id, ', ', tostring(k - 1), '\n'
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
    std::optional<std::reference_wrapper<emilua::native_module>>
    get_builtin_native_module(std::string_view id)
    {
        auto v = Perfect_Hash::in_word_set(id.data(), id.size());
        if (!v) {
            return std::nullopt;
        }

        return std::ref(*modules_indexes[v->action]);
    }

    void create_native_modules(
        const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
        app_context& appctx)
    {
    ]]
))

for k, v in ipairs(input) do
    stream.write_all(system.out, format(
        'plugin{0}.emplace();\n' ..
        'modules_indexes[{0}] = &*plugin{0};\n' ..
        'plugin{0}->init_appctx(modules_cache_registry_wlock, appctx);\n',
        k - 1))
end

stream.write_all(system.out, byte_span.append(
    [[
    }

    void destroy_native_modules()
    {
        modules_indexes.fill(nullptr);
    ]]
))

for k, v in ipairs(input) do
    stream.write_all(system.out, format('plugin{0}.reset();\n', k - 1))
end

stream.write_all(system.out, byte_span.append(
    [[
    }
    ]] ..
    '} // namespace emilua\n'
))
            )lua";
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
        fs::path{"/dev/null/app/init.lua", fs::path::generic_format});
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

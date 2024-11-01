// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/native_module.hpp>

namespace emilua {

class hello_plugin : native_module
{
public:
    std::error_code init_lua_module(
        std::shared_lock<std::shared_mutex>&, emilua::vm_context& /*vm_ctx*/,
        lua_State* L) override
    {
        lua_newtable(L);

        lua_pushcfunction(L, hello);
        lua_setfield(L, -2, "hello");

        return {};
    }

private:
    static int hello(lua_State* L);
};

int hello_plugin::hello(lua_State* L)
{
    lua_pushliteral(L, "hello");
    return 1;
}

} // namespace emilua

extern "C" BOOST_SYMBOL_EXPORT emilua::hello_plugin EMILUA_PLUGIN_SYMBOL;
emilua::hello_plugin EMILUA_PLUGIN_SYMBOL;

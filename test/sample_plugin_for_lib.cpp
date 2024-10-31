// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/plugin.hpp>

using emilua::push;

std::string foobar();

class plugin : emilua::plugin
{
public:
    std::error_code init_lua_module(
        std::shared_lock<std::shared_mutex>& /*modules_cache_registry_rlock*/,
        emilua::vm_context& /*vm_ctx*/, lua_State* L) override
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "foobar");
        lua_pushcfunction(L, [](lua_State* L) -> int {
            push(L, foobar());
            return 1;
        });
        lua_rawset(L, -3);

        return {};
    }
};

extern "C" BOOST_SYMBOL_EXPORT plugin EMILUA_PLUGIN_SYMBOL;
plugin EMILUA_PLUGIN_SYMBOL;

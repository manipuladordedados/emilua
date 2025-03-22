// Copyright (c) 2020 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char yield_reason_is_native_key;

enum FiberStatus: lua_Integer
{
    //RUNNING, //< same as not set/nil
    FINISHED_SUCCESSFULLY = 1,
    FINISHED_WITH_ERROR,
};

struct EMILUA_API fiber_handle
{
    fiber_handle(lua_State* fiber)
        : fiber{fiber}
        , interruption_caught{std::in_place_type_t<void>{}}
    {}

    lua_State* fiber;
    bool join_in_progress = false;
    result<bool, void> interruption_caught;
};

void init_fiber_module(lua_State* L);

EMILUA_API
void print_panic(const lua_State* L, bool is_main, std::string_view error,
                 std::string_view stacktrace);

EMILUA_API int set_current_traceback(lua_State* L);

} // namespace emilua

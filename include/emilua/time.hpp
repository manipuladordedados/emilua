// Copyright (c) 2020 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char time_key;
EMILUA_API extern char system_clock_time_point_mt_key;

void init_time(lua_State* L);

} // namespace emilua

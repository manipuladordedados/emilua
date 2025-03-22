// Copyright (c) 2023 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char filesystem_key;
EMILUA_API extern char filesystem_path_mt_key;
EMILUA_API extern char file_clock_time_point_mt_key;

void init_filesystem(lua_State* L);

} // namespace emilua

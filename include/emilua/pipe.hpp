// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char pipe_key;
EMILUA_API extern char readable_pipe_mt_key;
EMILUA_API extern char writable_pipe_mt_key;

void init_pipe(lua_State* L);

} // namespace emilua

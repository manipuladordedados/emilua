// Copyright (c) 2023 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char future_key;

void init_future(lua_State* L);

} // namespace emilua

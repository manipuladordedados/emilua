// Copyright (c) 2020 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char condition_variable_key;

void init_condition_variable_module(lua_State* L);

} // namespace emilua

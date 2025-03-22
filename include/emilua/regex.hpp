// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char regex_key;
EMILUA_API extern char regex_mt_key;

void init_regex(lua_State* L);
EMILUA_API const std::error_category& regex_category() noexcept;

} // namespace emilua

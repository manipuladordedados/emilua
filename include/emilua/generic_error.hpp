// Copyright (c) 2023 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char generic_error_key;

// returns 0 on error
int posix_errno_code_from_name(std::string_view name);

void init_generic_error(lua_State* L);

} // namespace emilua

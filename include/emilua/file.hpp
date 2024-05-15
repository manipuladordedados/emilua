// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char file_key;
extern char file_stream_mt_key;
extern char file_random_access_mt_key;

void init_file(lua_State* L);

} // namespace emilua

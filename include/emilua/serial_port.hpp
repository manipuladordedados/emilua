// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char serial_port_key;
extern char serial_port_mt_key;

void init_serial_port(lua_State* L);

} // namespace emilua

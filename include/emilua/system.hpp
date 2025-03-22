// Copyright (c) 2021 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

EMILUA_API extern char system_key;

#if BOOST_OS_LINUX
EMILUA_API extern char linux_capabilities_mt_key;
#endif // BOOST_OS_LINUX

void init_system(lua_State* L);

} // namespace emilua

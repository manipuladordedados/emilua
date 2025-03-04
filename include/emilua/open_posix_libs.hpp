// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <boost/predef/os/unix.h>

extern "C" {
#include <lauxlib.h>
#include <luajit.h>
#include <lualib.h>
#include <lua.h>
}

#if !BOOST_OS_UNIX
# error "posix libs only supported on POSIX-like systems"
#endif // !BOOST_OS_UNIX

namespace emilua {

void open_posix_libs(lua_State* L);

} // namespace emilua

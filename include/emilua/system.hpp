/* Copyright (c) 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char system_key;

#if BOOST_OS_LINUX
extern char linux_capabilities_mt_key;
#endif // BOOST_OS_LINUX

void init_system(lua_State* L);

} // namespace emilua

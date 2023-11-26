/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <emilua/core.hpp>

namespace emilua {

int system_spawn(lua_State* L)
{
    return throw_enosys(L);
}

void init_system_spawn(lua_State* L)
{
}

} // namespace emilua

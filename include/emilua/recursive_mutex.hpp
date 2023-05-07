/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char recursive_mutex_key;

void init_recursive_mutex_module(lua_State* L);

} // namespace emilua

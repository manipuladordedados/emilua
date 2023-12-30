/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char file_descriptor_mt_key;

#if BOOST_OS_WINDOWS
// When *this == INVALID_FILE_DESCRIPTOR: raise EBUSY
using file_descriptor_handle = HANDLE;

#define INVALID_FILE_DESCRIPTOR INVALID_HANDLE_VALUE
#else // BOOST_OS_WINDOWS
// When *this == INVALID_FILE_DESCRIPTOR: raise EBUSY
using file_descriptor_handle = int;

constexpr file_descriptor_handle INVALID_FILE_DESCRIPTOR = -1;
#endif // BOOST_OS_WINDOWS

void init_file_descriptor(lua_State* L);

} // namespace emilua

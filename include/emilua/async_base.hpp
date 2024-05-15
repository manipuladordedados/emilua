// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char var_args__retval1_to_error__fwd_retval2__key;
extern char var_args__retval1_to_error__key;
extern char var_args__retval1_to_error__fwd_retval234__key;
extern char var_args__retval1_to_error__fwd_retval23__key;

void init_async_base(lua_State* L);

} // namespace emilua

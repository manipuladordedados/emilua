/* Copyright (c) 2024 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

// Landlock structs and types are explicitly declared to avoid mismatched kernel
// headers and runtime. Landlock is designed with extensibility and backwards
// compatibility in mind. {{{
struct landlock_ruleset_attr {
    std::uint64_t handled_access_fs;
};

struct landlock_path_beneath_attr {
    std::uint64_t allowed_access;
    std::int32_t parent_fd;
} __attribute__((packed));
// }}}

namespace emilua::detail {

emilua::result<std::uint64_t, const char*>
landlock_handled_access_fs(lua_State* L);

} // namespace emilua::detail

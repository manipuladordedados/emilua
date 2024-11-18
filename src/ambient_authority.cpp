// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/ambient_authority.hpp>

namespace emilua {

#if BOOST_OS_UNIX
# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
bool has_libc_service = false;

struct ambient_authority ambient_authority;
#endif // BOOST_OS_UNIX

} // namespace emilua

// Copyright (c) 2020 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {
namespace detail {

EMILUA_API extern char context_key;
EMILUA_API extern char error_code_mt_key;
EMILUA_API extern char error_category_mt_key;
EMILUA_API extern char rdf_error_category_module_mt_key;

} // namespace detail
} // namespace emilua

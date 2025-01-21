// Copyright (c) 2020 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/io_context.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/io_context.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua {

// You may override these functions (they're weak symbols in static buidls) to
// initialize your native modules from subprocess-based actors as well. {{{
void create_native_modules(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx);

void destroy_native_modules();
// }}}

std::shared_ptr<vm_context> make_vm(
    asio::io_context& ioctx,
    emilua::app_context& appctx,
    ContextType lua_context,
    std::filesystem::path entry_point,
    std::filesystem::path import_root = std::filesystem::path{});

} // namespace emilua

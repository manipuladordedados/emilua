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
#if BOOST_OS_WINDOWS
extern
void (*create_native_modules)(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx);

extern
void (*destroy_native_modules)();
#else // BOOST_OS_WINDOWS
void create_native_modules(
    const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
    app_context& appctx);

void destroy_native_modules();
#endif // BOOST_OS_WINDOWS
// }}}

std::shared_ptr<vm_context> make_vm(
    emilua::app_context& appctx,
    asio::io_context& ioctx,
    ContextType lua_context,
    std::filesystem::path entry_point,
    std::filesystem::path import_root = std::filesystem::path{},
    std::optional<strand_type> strand = std::nullopt,
    std::shared_ptr<void> memory_resource = nullptr,
    std::size_t memory_resource_size = 0);

} // namespace emilua

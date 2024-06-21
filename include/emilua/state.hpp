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

std::shared_ptr<vm_context> make_vm(
    asio::io_context& ioctx,
    emilua::app_context& appctx,
    ContextType lua_context,
    std::filesystem::path entry_point,
    std::filesystem::path import_root = std::filesystem::path{});

} // namespace emilua

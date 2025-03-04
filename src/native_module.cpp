// Copyright (c) 2021, 2023, 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/native_module.hpp>

namespace emilua {

void native_module::init_appctx(
    const std::unique_lock<std::shared_mutex>&, app_context&) noexcept
{}

std::error_code native_module::init_ioctx_services(
    std::shared_lock<std::shared_mutex>&, asio::io_context&) noexcept
{
    return {};
}

std::error_code native_module::init_lua_module(
    std::shared_lock<std::shared_mutex>&, vm_context&, lua_State*)
{
    return errc::internal_module;
}

} // namespace emilua

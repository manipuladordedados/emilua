#include <emilua/plugin.hpp>

namespace emilua {

void plugin::init_appctx(
    const std::unique_lock<std::shared_mutex>&, app_context&) noexcept
{}

std::error_code plugin::init_ioctx_services(
    std::shared_lock<std::shared_mutex>&, asio::io_context&) noexcept
{
    return {};
}

std::error_code plugin::init_lua_module(
    std::shared_lock<std::shared_mutex>&, vm_context&, lua_State*)
{
    return errc::internal_module;
}

} // namespace emilua

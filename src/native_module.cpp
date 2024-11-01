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

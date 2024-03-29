/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <boost/asio/serial_port.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/file_descriptor.hpp>
#include <emilua/serial_port.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char serial_port_key;
char serial_port_mt_key;

EMILUA_GPERF_DECLS_BEGIN(serial_port)
EMILUA_GPERF_NAMESPACE(emilua)
static char serial_port_read_some_key;
static char serial_port_write_some_key;

static int serial_port_open(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

#if BOOST_OS_UNIX
    // Boost.Asio unfortunately ruins the state of whatever serial port you try
    // to open by setting "default serial port options" without ever asking the
    // user whether he wanted that. We skip all this crap by opening the device
    // ourselves.
    int fd = open(lua_tostring(L, 2), O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (fd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->assign(fd, ec);
    assert(!ec); boost::ignore_unused(ec);
    return 0;
#else
    boost::system::error_code ec;
    port->open(static_cast<std::string>(tostringview(L, 2)), ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
#endif // BOOST_OS_UNIX
}

static int serial_port_close(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int serial_port_cancel(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int serial_port_assign(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 2));
    if (!handle || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    lua_pushnil(L);
    setmetatable(L, 2);

    boost::system::error_code ec;
    port->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 0;
}

static int serial_port_release(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (port->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    // https://github.com/chriskohlhoff/asio/issues/1182
    int newfd = dup(port->native_handle());
    BOOST_SCOPE_EXIT_ALL(&) {
        if (newfd != -1) {
            int res = close(newfd);
            boost::ignore_unused(res);
        }
    };
    if (newfd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    boost::system::error_code ignored_ec;
    port->close(ignored_ec);

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = newfd;
    newfd = -1;
    return 1;
}

static int serial_port_isatty(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (port->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    lua_pushboolean(L, isatty(port->native_handle()));
    return 1;
}

static int serial_port_tcgetpgrp(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (port->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    pid_t res = tcgetpgrp(port->native_handle());
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    lua_pushnumber(L, res);
    return 1;
}

static int serial_port_tcsetpgrp(lua_State* L)
{
    lua_settop(L, 2);

    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (port->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    if (tcsetpgrp(port->native_handle(), luaL_checknumber(L, 2)) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_UNIX

static int serial_port_send_break(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    port->send_break(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}
EMILUA_GPERF_DECLS_END(serial_port)

static int serial_port_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    port->async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int serial_port_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    if (!port || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 2));
    if (!bs || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    port->async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(serial_port)
EMILUA_GPERF_NAMESPACE(emilua)
inline int serial_port_is_open(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    lua_pushboolean(L, port->is_open());
    return 1;
}
EMILUA_GPERF_DECLS_END(serial_port)

static int serial_port_mt_newindex(lua_State* L)
{
    auto port = static_cast<asio::serial_port*>(lua_touserdata(L, 1));
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, asio::serial_port*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, asio::serial_port*) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "baud_rate",
            [](lua_State* L, asio::serial_port* port) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::serial_port_base::baud_rate o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                port->set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "flow_control",
            [](lua_State* L, asio::serial_port* port) -> int {
                switch (lua_type(L, 3)) {
                case LUA_TNIL: {
                    asio::serial_port_base::flow_control o{
                        asio::serial_port_base::flow_control::none};
                    boost::system::error_code ec;
                    port->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
                case LUA_TSTRING:
                    break;
                default:
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }

                auto value = tostringview(L, 3);
                auto o = EMILUA_GPERF_BEGIN(value)
                    EMILUA_GPERF_PARAM(
                        asio::serial_port_base::flow_control::type action)
                    EMILUA_GPERF_PAIR(
                        "software",
                        asio::serial_port_base::flow_control::software)
                    EMILUA_GPERF_PAIR(
                        "hardware",
                        asio::serial_port_base::flow_control::hardware)
                EMILUA_GPERF_END(value);
                if (!o) {
                    push(L, std::errc::not_supported);
                    return lua_error(L);
                }

                boost::system::error_code ec;
                port->set_option(asio::serial_port_base::flow_control{*o}, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "parity",
            [](lua_State* L, asio::serial_port* port) -> int {
                switch (lua_type(L, 3)) {
                case LUA_TNIL: {
                    asio::serial_port_base::parity o{
                        asio::serial_port_base::parity::none};
                    boost::system::error_code ec;
                    port->set_option(o, ec);
                    if (ec) {
                        push(L, static_cast<std::error_code>(ec));
                        return lua_error(L);
                    }
                    return 0;
                }
                case LUA_TSTRING:
                    break;
                default:
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }

                auto value = tostringview(L, 3);
                auto o = EMILUA_GPERF_BEGIN(value)
                    EMILUA_GPERF_PARAM(
                        asio::serial_port_base::parity::type action)
                    EMILUA_GPERF_PAIR(
                        "odd", asio::serial_port_base::parity::odd)
                    EMILUA_GPERF_PAIR(
                        "even", asio::serial_port_base::parity::even)
                EMILUA_GPERF_END(value);
                if (!o) {
                    push(L, std::errc::not_supported);
                    return lua_error(L);
                }

                boost::system::error_code ec;
                port->set_option(asio::serial_port_base::parity{*o}, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "stop_bits",
            [](lua_State* L, asio::serial_port* port) -> int {
                luaL_checktype(L, 3, LUA_TSTRING);

                auto value = tostringview(L, 3);
                auto o = EMILUA_GPERF_BEGIN(value)
                    EMILUA_GPERF_PARAM(
                        asio::serial_port_base::stop_bits::type action)
                    EMILUA_GPERF_PAIR(
                        "one", asio::serial_port_base::stop_bits::one)
                    EMILUA_GPERF_PAIR(
                        "one_point_five",
                        asio::serial_port_base::stop_bits::onepointfive)
                    EMILUA_GPERF_PAIR(
                        "two", asio::serial_port_base::stop_bits::two)
                EMILUA_GPERF_END(value);
                if (!o) {
                    push(L, std::errc::not_supported);
                    return lua_error(L);
                }

                boost::system::error_code ec;
                port->set_option(asio::serial_port_base::stop_bits{*o}, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "character_size",
            [](lua_State* L, asio::serial_port* port) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::serial_port_base::character_size o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                port->set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
    EMILUA_GPERF_END(key)(L, port);
}

static int serial_port_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "open",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, serial_port_open);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, serial_port_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, serial_port_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, serial_port_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, serial_port_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "isatty",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, serial_port_isatty);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "tcgetpgrp",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, serial_port_tcgetpgrp);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "tcsetpgrp",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, serial_port_tcsetpgrp);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_break",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, serial_port_send_break);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &serial_port_read_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &serial_port_write_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", serial_port_is_open)
        EMILUA_GPERF_PAIR(
            "baud_rate",
            [](lua_State* L) -> int {
                auto port = static_cast<asio::serial_port*>(
                    lua_touserdata(L, 1));
                asio::serial_port_base::baud_rate o;
                boost::system::error_code ec;
                port->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "flow_control",
            [](lua_State* L) -> int {
                auto port = static_cast<asio::serial_port*>(
                    lua_touserdata(L, 1));
                asio::serial_port_base::flow_control o;
                boost::system::error_code ec;
                port->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                switch (o.value()) {
                case asio::serial_port_base::flow_control::none:
                    lua_pushnil(L);
                    break;
                case asio::serial_port_base::flow_control::software:
                    lua_pushliteral(L, "software");
                    break;
                case asio::serial_port_base::flow_control::hardware:
                    lua_pushliteral(L, "hardware");
                }
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "parity",
            [](lua_State* L) -> int {
                auto port = static_cast<asio::serial_port*>(
                    lua_touserdata(L, 1));
                asio::serial_port_base::parity o;
                boost::system::error_code ec;
                port->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                switch (o.value()) {
                case asio::serial_port_base::parity::none:
                    lua_pushnil(L);
                    break;
                case asio::serial_port_base::parity::odd:
                    lua_pushliteral(L, "odd");
                    break;
                case asio::serial_port_base::parity::even:
                    lua_pushliteral(L, "even");
                }
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "stop_bits",
            [](lua_State* L) -> int {
                auto port = static_cast<asio::serial_port*>(
                    lua_touserdata(L, 1));
                asio::serial_port_base::stop_bits o;
                boost::system::error_code ec;
                port->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                switch (o.value()) {
                case asio::serial_port_base::stop_bits::one:
                    lua_pushliteral(L, "one");
                    break;
                case asio::serial_port_base::stop_bits::onepointfive:
                    lua_pushliteral(L, "one_point_five");
                    break;
                case asio::serial_port_base::stop_bits::two:
                    lua_pushliteral(L, "two");
                }
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "character_size",
            [](lua_State* L) -> int {
                auto port = static_cast<asio::serial_port*>(
                    lua_touserdata(L, 1));
                asio::serial_port_base::character_size o;
                boost::system::error_code ec;
                port->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int serial_port_new(lua_State* L)
{
    int nargs = lua_gettop(L);
    auto& vm_ctx = get_vm_context(L);

    if (nargs == 0) {
        auto port = static_cast<asio::serial_port*>(
            lua_newuserdata(L, sizeof(asio::serial_port))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
        setmetatable(L, -2);
        new (port) asio::serial_port{vm_ctx.strand().context()};
        return 1;
    }

#if BOOST_OS_UNIX
    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    auto port = static_cast<asio::serial_port*>(
        lua_newuserdata(L, sizeof(asio::serial_port))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    setmetatable(L, -2);
    new (port) asio::serial_port{vm_ctx.strand().context()};

    lua_pushnil(L);
    setmetatable(L, 1);

    boost::system::error_code ec;
    port->assign(*handle, ec);
    assert(!ec); boost::ignore_unused(ec);

    return 1;
#else // BOOST_OS_UNIX
    push(L, std::errc::invalid_argument, "arg", 1);
    return lua_error(L);
#endif // BOOST_OS_UNIX
}

#if BOOST_OS_UNIX
static int serial_ptypair(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    int masterfd = posix_openpt(O_RDWR | O_NOCTTY);
    if (masterfd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) { if (masterfd != -1) { close(masterfd); } };

    if (grantpt(masterfd) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    if (unlockpt(masterfd) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    char* slavepath = ptsname(masterfd);
    if (slavepath == NULL) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    int slavefd = open(slavepath, O_RDWR | O_NONBLOCK | O_NOCTTY);
    if (slavefd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) { if (slavefd != -1) { close(slavefd); } };

    auto master = static_cast<asio::serial_port*>(
        lua_newuserdata(L, sizeof(asio::serial_port))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &serial_port_mt_key);
    setmetatable(L, -2);
    new (master) asio::serial_port{vm_ctx.strand().context()};

    boost::system::error_code ec;
    master->assign(masterfd, ec);
    assert(!ec); boost::ignore_unused(ec);
    masterfd = -1;

    auto slave = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);
    *slave = slavefd;
    slavefd = -1;

    return 2;
}
#endif // BOOST_OS_UNIX

void init_serial_port(lua_State* L)
{
    lua_pushlightuserdata(L, &serial_port_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "new");
        lua_pushcfunction(L, serial_port_new);
        lua_rawset(L, -3);

#if BOOST_OS_UNIX
        lua_pushliteral(L, "ptypair");
        lua_pushcfunction(L, serial_ptypair);
        lua_rawset(L, -3);
#endif // BOOST_OS_UNIX
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &serial_port_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/4);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "serial_port");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, serial_port_mt_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, serial_port_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::serial_port>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &serial_port_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, serial_port_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &serial_port_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, serial_port_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

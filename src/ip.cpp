/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <boost/asio/ip/host_name.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/unicast.hpp>
#include <boost/asio/ip/v6_only.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/scope_exit.hpp>

#include <charconv>

#include <emilua/file_descriptor.hpp>
#include <emilua/async_base.hpp>
#include <emilua/byte_span.hpp>
#include <emilua/ip.hpp>

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
#include <boost/asio/windows/object_handle.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/scope_exit.hpp>
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
#include <boost/asio/windows/overlapped_ptr.hpp>
#include <boost/asio/random_access_file.hpp>
#include <emilua/file.hpp>
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

extern unsigned char ip_connect_bytecode[];
extern std::size_t ip_connect_bytecode_size;

char ip_key;
char ip_address_mt_key;
char ip_tcp_socket_mt_key;
char ip_tcp_acceptor_mt_key;
char ip_udp_socket_mt_key;

EMILUA_GPERF_DECLS_BEGIN(ip)
EMILUA_GPERF_NAMESPACE(emilua)
static char tcp_socket_connect_key;
static char tcp_socket_read_some_key;
static char tcp_socket_write_some_key;
static char tcp_socket_receive_key;
static char tcp_socket_send_key;
static char tcp_socket_wait_key;
static char tcp_acceptor_accept_key;
static char udp_socket_connect_key;
static char udp_socket_receive_key;
static char udp_socket_receive_from_key;
static char udp_socket_send_key;
static char udp_socket_send_to_key;
EMILUA_GPERF_DECLS_END(ip)

EMILUA_GPERF_DECLS_BEGIN(ip)
EMILUA_GPERF_NAMESPACE(emilua)
#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
static char tcp_socket_send_file_key;
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
EMILUA_GPERF_DECLS_END(ip)

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
struct get_address_info_context_t: public pending_operation
{
    get_address_info_context_t(asio::io_context& ctx)
        : pending_operation{/*shared_ownership=*/true}
        , hCompletion(ctx)
    {
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
    }

    void cancel() noexcept override
    {
        GetAddrInfoExCancel(&hCancel);
    }

    ~get_address_info_context_t()
    {
        // WARNING: If you call `asio::io_context::stop()` the handler will be
        // destroyed before `hCompletion->native_handle()` has been set (and
        // then we hit this very destructor). The underlying asynchronous
        // operation will not stop along with it. The underlying asynchronous
        // operation will be writing to this freed memory and a random memory
        // corruption will occur. If we're **lucky** this `assert()` will
        // trigger.
        //
        // Are you reading this code because the `assert()` triggered? Remove
        // the call to `asio::io_context::stop()` from your code to fix the
        // problem.
        //
        // A different approach would be to use the `lpCompletionRoutine`
        // parameter from `GetAddrInfoExW()` and take care of all Boost.Asio
        // integration ourselves (i.e. skip `asio::windows::object_handle`
        // altogether). However (1) this would be more work and (2) so far
        // Emilua has not been designed to rely on `asio::io_context::stop()` to
        // begin with. Therefore we can safely use the simpler approach
        // (i.e. remove `asio::io_context::stop()` from your new buggy code).
        //
        // Maybe you'll think that yet another approach would be to cancel the
        // operation right here and sync with it. This approach would have other
        // problems for which I won't write any prose.
        assert(results == nullptr);
    }

    asio::windows::object_handle hCompletion;
    OVERLAPPED overlapped;
    PADDRINFOEXW results = nullptr;
    HANDLE hCancel = nullptr;
};
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

struct resolver_service: public pending_operation
{
    resolver_service(asio::io_context& ioctx)
        : pending_operation{/*shared_ownership=*/false}
        , tcp_resolver{ioctx}
        , udp_resolver{ioctx}
    {}

    void cancel() noexcept override
    {}

    asio::ip::tcp::resolver tcp_resolver;
    asio::ip::udp::resolver udp_resolver;
};

static int ip_host_name(lua_State* L)
{
    boost::system::error_code ec;
    auto val = asio::ip::host_name(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    push(L, val);
    return 1;
}

static int ip_tostring(lua_State* L)
{
    int nargs = lua_gettop(L);

    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = a->to_string();

    switch (nargs) {
    case 1:
        break;
    case 2: {
        std::uint16_t port = luaL_checkinteger(L, 2);
        std::array<char, 5> buf;
        auto s_size = std::to_chars(
            buf.data(),
            buf.data() + buf.size(),
            port).ptr - buf.data();
        if (a->is_v4()) {
            ret.reserve(ret.size() + 1 + s_size);
            ret.push_back(':');
        } else {
            ret.reserve(ret.size() + 3 + s_size);
            ret.insert(ret.begin(), '[');
            ret.append("]:");
        }
        ret.append(buf.data(), s_size);
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    push(L, ret);
    return 1;
}

static int ip_toendpoint(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TSTRING);

    std::string_view host = tostringview(L, 1);
    std::uint16_t port;

    {
        auto idx = host.rfind(':');
        if (idx == std::string_view::npos) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        std::string_view p = host.substr(idx + 1);
        if (p.starts_with("0") && p.size() != 1) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        auto res = std::from_chars(p.data(), p.data() + p.size(), port);
        if (res.ec != std::errc{}) {
            push(L, res.ec, "arg", 1);
            return lua_error(L);
        } else if (res.ptr != p.data() + p.size()) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        host.remove_suffix(p.size() + 1);
    }

    bool is_ipv6 = false;
    if (host.starts_with("[")) {
        if (host.back() != ']') {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        host.remove_suffix(1);
        host.remove_prefix(1);
        is_ipv6 = true;
    }

    auto addr = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);

    boost::system::error_code ec;
    new (addr) asio::ip::address{asio::ip::make_address(host, ec)};
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }

    if ((is_ipv6 && addr->is_v4()) || (!is_ipv6 && addr->is_v6())) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushinteger(L, port);

    return 2;
}

static int address_new(lua_State* L)
{
    lua_settop(L, 1);
    auto a = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    switch (lua_type(L, 1)) {
    case LUA_TNIL:
        new (a) asio::ip::address{};
        break;
    case LUA_TSTRING: {
        boost::system::error_code ec;
        new (a) asio::ip::address{
            asio::ip::make_address(lua_tostring(L, 1), ec)
        };
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        break;
    }
    default:
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    return 1;
}

static int address_any_v4(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v4::any()};
    return 1;
}

static int address_any_v6(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v6::any()};
    return 1;
}

static int address_loopback_v4(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v4::loopback()};
    return 1;
}

static int address_loopback_v6(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v6::loopback()};
    return 1;
}

static int address_broadcast_v4(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::address{asio::ip::address_v4::broadcast()};
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(address)
EMILUA_GPERF_NAMESPACE(emilua)
static int address_to_v6(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2) || !a->is_v4()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (ret) asio::ip::address{
        asio::ip::make_address_v6(asio::ip::v4_mapped, a->to_v4())
    };

    return 1;
}

static int address_to_v4(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2) || !a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    try {
        new (ret) asio::ip::address{
            asio::ip::make_address_v4(asio::ip::v4_mapped, a->to_v6())
        };
    } catch (const asio::ip::bad_address_cast&) {
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);
        push(L, std::errc::invalid_argument);
        return lua_error(L);
    }

    return 1;
}

inline int address_is_loopback(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_loopback());
    return 1;
}

inline int address_is_multicast(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_multicast());
    return 1;
}

inline int address_is_unspecified(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_unspecified());
    return 1;
}

inline int address_is_v4(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_v4());
    return 1;
}

inline int address_is_v6(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_v6());
    return 1;
}

inline int address_is_link_local(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_link_local());
    return 1;
}

inline int address_is_multicast_global(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_global());
    return 1;
}

inline int address_is_multicast_link_local(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_link_local());
    return 1;
}

inline int address_is_multicast_node_local(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_node_local());
    return 1;
}

inline int address_is_multicast_org_local(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_org_local());
    return 1;
}

inline int address_is_multicast_site_local(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_multicast_site_local());
    return 1;
}

inline int address_is_site_local(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_site_local());
    return 1;
}

inline int address_is_v4_mapped(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, a->to_v6().is_v4_mapped());
    return 1;
}

inline int address_scope_id_get(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushnumber(L, a->to_v6().scope_id());
    return 1;
}
EMILUA_GPERF_DECLS_END(address)

static int address_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "to_v6",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, address_to_v6);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "to_v4",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, address_to_v4);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_loopback", address_is_loopback)
        EMILUA_GPERF_PAIR("is_multicast", address_is_multicast)
        EMILUA_GPERF_PAIR("is_unspecified", address_is_unspecified)
        EMILUA_GPERF_PAIR("is_v4", address_is_v4)
        EMILUA_GPERF_PAIR("is_v6", address_is_v6)

        // v6-only properties
        EMILUA_GPERF_PAIR("is_link_local", address_is_link_local)
        EMILUA_GPERF_PAIR("is_multicast_global", address_is_multicast_global)
        EMILUA_GPERF_PAIR(
            "is_multicast_link_local", address_is_multicast_link_local)
        EMILUA_GPERF_PAIR(
            "is_multicast_node_local", address_is_multicast_node_local)
        EMILUA_GPERF_PAIR(
            "is_multicast_org_local", address_is_multicast_org_local)
        EMILUA_GPERF_PAIR(
            "is_multicast_site_local", address_is_multicast_site_local)
        EMILUA_GPERF_PAIR("is_site_local", address_is_site_local)
        EMILUA_GPERF_PAIR("is_v4_mapped", address_is_v4_mapped)
        EMILUA_GPERF_PAIR("scope_id", address_scope_id_get)
    EMILUA_GPERF_END(key)(L);
}

EMILUA_GPERF_DECLS_BEGIN(address)
EMILUA_GPERF_NAMESPACE(emilua)
inline int address_scope_id_set(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a->is_v6()) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto as_v6 = a->to_v6();
    as_v6.scope_id(lua_tonumber(L, 3));
    *a = as_v6;
    return 0;
}
EMILUA_GPERF_DECLS_END(address)

static int address_mt_newindex(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR("scope_id", address_scope_id_set)
    EMILUA_GPERF_END(key)(L);
}

static int address_mt_tostring(lua_State* L)
{
    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto ret = a->to_string();
    lua_pushlstring(L, ret.data(), ret.size());
    return 1;
}

static int address_mt_eq(lua_State* L)
{
    auto a1 = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    auto a2 = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *a1 == *a2);
    return 1;
}

static int address_mt_lt(lua_State* L)
{
    auto a1 = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto a2 = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
    if (!a2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *a1 < *a2);
    return 1;
}

static int address_mt_le(lua_State* L)
{
    auto a1 = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto a2 = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
    if (!a2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *a1 <= *a2);
    return 1;
}

static int tcp_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = static_cast<tcp_socket*>(lua_newuserdata(L, sizeof(tcp_socket)));
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    setmetatable(L, -2);
    new (a) tcp_socket{vm_ctx.strand().context()};
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(tcp_socket)
EMILUA_GPERF_NAMESPACE(emilua)
static int tcp_socket_open(lua_State* L)
{
    lua_settop(L, 2);
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        sock->socket.open(asio::ip::tcp::endpoint{*addr, 0}.protocol(), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        auto key = tostringview(L, 2);
        auto protocol = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(asio::ip::tcp action)
            EMILUA_GPERF_PAIR("v4", asio::ip::tcp::v4())
            EMILUA_GPERF_PAIR("v6", asio::ip::tcp::v6())
        EMILUA_GPERF_END(key);

        if (!protocol) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        sock->socket.open(*protocol, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_socket_bind(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        asio::ip::tcp::endpoint ep(*addr, lua_tointeger(L, 3));
        boost::system::error_code ec;
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        boost::system::error_code ec;
        auto addr = asio::ip::make_address(lua_tostring(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        asio::ip::tcp::endpoint ep(addr, lua_tointeger(L, 3));
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_socket_close(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int tcp_socket_cancel(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int tcp_socket_assign(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 3));
    if (!handle || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        lua_pushnil(L);
        setmetatable(L, 3);

        boost::system::error_code ec;
        sock->socket.assign(
            asio::ip::tcp::endpoint{*addr, 0}.protocol(), *handle, ec);
        assert(!ec); boost::ignore_unused(ec);

        return 0;
    }
    case LUA_TSTRING: {
        auto key = tostringview(L, 2);
        auto protocol = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(asio::ip::tcp action)
            EMILUA_GPERF_PAIR("v4", asio::ip::tcp::v4())
            EMILUA_GPERF_PAIR("v6", asio::ip::tcp::v6())
        EMILUA_GPERF_END(key);

        if (!protocol) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        lua_pushnil(L);
        setmetatable(L, 3);

        boost::system::error_code ec;
        sock->socket.assign(*protocol, *handle, ec);
        assert(!ec); boost::ignore_unused(ec);

        return 0;
    }
    }
}

static int tcp_socket_release(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    int rawfd = sock->socket.release(ec);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
            int res = close(rawfd);
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}
#endif // BOOST_OS_UNIX

static int tcp_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, tcp_socket*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, tcp_socket*) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "bytes_readable",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::bytes_readable command;
                boost::system::error_code ec;
                socket->socket.io_control(command, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushnumber(L, command.get());
                return 1;
            })
    EMILUA_GPERF_END(key)(L, socket);
}

static int tcp_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    auto what = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(asio::ip::tcp::socket::shutdown_type action)
        EMILUA_GPERF_PAIR("receive", asio::ip::tcp::socket::shutdown_receive)
        EMILUA_GPERF_PAIR("send", asio::ip::tcp::socket::shutdown_send)
        EMILUA_GPERF_PAIR("both", asio::ip::tcp::socket::shutdown_both)
    EMILUA_GPERF_END(key);
    if (!what) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    boost::system::error_code ec;
    socket->socket.shutdown(*what, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int tcp_socket_disconnect(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

#if BOOST_OS_WINDOWS
    push(L, std::errc::function_not_supported);
    return lua_error(L);
#else
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_UNSPEC;
    int res = connect(
        sock->socket.native_handle(), reinterpret_cast<struct sockaddr*>(&sin),
        sizeof(sin));
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
#endif // BOOST_OS_WINDOWS
}
EMILUA_GPERF_DECLS_END(tcp_socket)

static int tcp_socket_connect(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
    if (!a || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    asio::ip::tcp::endpoint ep{
        *a, static_cast<std::uint16_t>(lua_tointeger(L, 3))};

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_connect(ep, asio::bind_cancellation_slot(cancel_slot,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int tcp_socket_read_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
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

    ++s->nbusy;
    s->socket.async_read_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

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

static int tcp_socket_write_some(lua_State* L)
{
    lua_settop(L, 2);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
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

    ++s->nbusy;
    s->socket.async_write_some(
        asio::buffer(bs->data.get(), bs->size),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

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

static int tcp_socket_receive(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
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

    ++s->nbusy;
    s->socket.async_receive(
        asio::buffer(bs->data.get(), bs->size),
        lua_tointeger(L, 3),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

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

static int tcp_socket_send(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
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

    ++s->nbusy;
    s->socket.async_send(
        asio::buffer(bs->data.get(), bs->size),
        lua_tointeger(L, 3),
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,s](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

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

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
// https://www.boost.org/doc/libs/1_78_0/doc/html/boost_asio/example/cpp03/windows/transmit_file.cpp
static int tcp_socket_send_file(lua_State* L)
{
    lua_settop(L, 7);
    luaL_checktype(L, 3, LUA_TNUMBER);
    luaL_checktype(L, 4, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto file = static_cast<asio::random_access_file*>(
        lua_touserdata(L, 2));
    if (!file || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_random_access_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    TRANSMIT_FILE_BUFFERS transmitBuffers;
    std::shared_ptr<unsigned char[]> buf1, buf2;
    lua_Integer n_number_of_bytes_per_send;

    if (lua_type(L, 5) != LUA_TNIL) {
        auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 5));
        if (!bs || !lua_getmetatable(L, 5)) {
            push(L, std::errc::invalid_argument, "arg", 5);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 5);
            return lua_error(L);
        }
        buf1 = bs->data;
        transmitBuffers.Head = bs->data.get();
        transmitBuffers.HeadLength = bs->size;
    } else {
        transmitBuffers.Head = NULL;
        transmitBuffers.HeadLength = 0;
    }

    if (lua_type(L, 6) != LUA_TNIL) {
        auto bs = static_cast<byte_span_handle*>(lua_touserdata(L, 6));
        if (!bs || !lua_getmetatable(L, 6)) {
            push(L, std::errc::invalid_argument, "arg", 6);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &byte_span_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 6);
            return lua_error(L);
        }
        buf2 = bs->data;
        transmitBuffers.Tail = bs->data.get();
        transmitBuffers.TailLength = bs->size;
    } else {
        transmitBuffers.Tail = NULL;
        transmitBuffers.TailLength = 0;
    }

    switch (lua_type(L, 7)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 7);
        return lua_error(L);
    case LUA_TNUMBER:
        n_number_of_bytes_per_send = lua_tointeger(L, 7);
        break;
    case LUA_TNIL:
        n_number_of_bytes_per_send = 0;
    }

    asio::windows::overlapped_ptr overlapped{
        vm_ctx->strand_using_defer(),
        [vm_ctx,current_fiber,buf1,buf2,sock](
            const boost::system::error_code& ec,
            std::size_t bytes_transferred
        ) {
            if (!vm_ctx->valid())
                return;

            --sock->nbusy;

            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(ec, bytes_transferred))));
        }
    };

    DWORD64 offset = lua_tointeger(L, 3);
    overlapped.get()->Offset = static_cast<DWORD>(offset);
    overlapped.get()->OffsetHigh = static_cast<DWORD>(offset >> 32);

    lua_pushvalue(L, 1);
    lua_pushlightuserdata(L, overlapped.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto sock = static_cast<tcp_socket*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            CancelIoEx(
                reinterpret_cast<HANDLE>(
                    static_cast<SOCKET>(sock->socket.native_handle())),
                static_cast<LPOVERLAPPED>(
                    lua_touserdata(L, lua_upvalueindex(2))));
            return 0;
        },
        2);
    set_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    BOOL ok = TransmitFile(sock->socket.native_handle(), file->native_handle(),
                           /*nNumberOfBytesToWrite=*/lua_tointeger(L, 4),
                           n_number_of_bytes_per_send,
                           overlapped.get(), &transmitBuffers,
                           /*dwReserved=*/0);
    DWORD last_error = GetLastError();

    // Check if the operation completed immediately.
    if (!ok && last_error != ERROR_IO_PENDING) {
        // The operation completed immediately, so a completion notification
        // needs to be posted. When complete() is called, ownership of the
        // OVERLAPPED-derived object passes to the io_context.
        boost::system::error_code ec(last_error,
                                     asio::error::get_system_category());
        overlapped.complete(ec, 0);
    } else {
        // The operation was successfully initiated, so ownership of the
        // OVERLAPPED-derived object has passed to the io_context.
        overlapped.release();
    }

    return lua_yield(L, 0);
}
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO

static int tcp_socket_wait(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    auto wait_type = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(asio::ip::tcp::socket::wait_type action)
        EMILUA_GPERF_PAIR("read", asio::ip::tcp::socket::wait_read)
        EMILUA_GPERF_PAIR("write", asio::ip::tcp::socket::wait_write)
        EMILUA_GPERF_PAIR("error", asio::ip::tcp::socket::wait_error)
    EMILUA_GPERF_END(key);
    if (!wait_type) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_wait(
        *wait_type,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(tcp_socket)
EMILUA_GPERF_NAMESPACE(emilua)
static int tcp_socket_set_option(lua_State* L)
{
    lua_settop(L, 4);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, tcp_socket*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, tcp_socket*) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "tcp_no_delay",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::ip::tcp::no_delay o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "send_low_watermark",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::socket_base::send_low_watermark o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "send_buffer_size",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "receive_low_watermark",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::socket_base::receive_low_watermark o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "receive_buffer_size",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::socket_base::receive_buffer_size o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "out_of_band_inline",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::out_of_band_inline o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "linger",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                luaL_checktype(L, 4, LUA_TNUMBER);
                asio::socket_base::linger o(
                    lua_toboolean(L, 3), lua_tointeger(L, 4));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "keep_alive",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::keep_alive o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "do_not_route",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::do_not_route o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "debug",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::debug o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "v6_only",
            [](lua_State* L, tcp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::ip::v6_only o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
    EMILUA_GPERF_END(key)(L, socket);
}

static int tcp_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, tcp_socket* socket))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, tcp_socket* socket) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "tcp_no_delay",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::ip::tcp::no_delay o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_low_watermark",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::send_low_watermark o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_buffer_size",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::send_buffer_size o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "receive_low_watermark",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::receive_low_watermark o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "receive_buffer_size",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::receive_buffer_size o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "out_of_band_inline",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::out_of_band_inline o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "linger",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::linger o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.enabled());
                lua_pushinteger(L, o.timeout());
                return 2;
            })
        EMILUA_GPERF_PAIR(
            "keep_alive",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::keep_alive o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "do_not_route",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::do_not_route o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "debug",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::socket_base::debug o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "v6_only",
            [](lua_State* L, tcp_socket* socket) -> int {
                asio::ip::v6_only o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
    EMILUA_GPERF_END(key)(L, socket);
}

inline int tcp_socket_is_open(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int tcp_socket_local_address(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int tcp_socket_local_port(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

inline int tcp_socket_remote_address(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int tcp_socket_remote_port(lua_State* L)
{
    auto sock = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

inline int tcp_socket_at_mark(lua_State* L)
{
    auto socket = static_cast<tcp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    bool ret = socket->socket.at_mark(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}
EMILUA_GPERF_DECLS_END(tcp_socket)

static int tcp_socket_mt_index(lua_State* L)
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
                lua_pushcfunction(L, tcp_socket_open);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "bind",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_bind);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, tcp_socket_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, tcp_socket_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "io_control",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_io_control);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "shutdown",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_shutdown);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "disconnect",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_disconnect);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "connect",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_connect_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_read_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "write_some",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_write_some_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "receive",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_receive_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_send_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_file",
            [](lua_State* L) -> int {
#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_send_file_key);
#else // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "wait",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_socket_wait_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "set_option",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_set_option);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "get_option",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_socket_get_option);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", tcp_socket_is_open)
        EMILUA_GPERF_PAIR("local_address", tcp_socket_local_address)
        EMILUA_GPERF_PAIR("local_port", tcp_socket_local_port)
        EMILUA_GPERF_PAIR("remote_address", tcp_socket_remote_address)
        EMILUA_GPERF_PAIR("remote_port", tcp_socket_remote_port)
        EMILUA_GPERF_PAIR("at_mark", tcp_socket_at_mark)
    EMILUA_GPERF_END(key)(L);
}

static int tcp_acceptor_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = static_cast<asio::ip::tcp::acceptor*>(
        lua_newuserdata(L, sizeof(asio::ip::tcp::acceptor))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    setmetatable(L, -2);
    new (a) asio::ip::tcp::acceptor{vm_ctx.strand().context()};
    return 1;
}

static int tcp_acceptor_accept(lua_State* L)
{
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    acceptor->async_accept(
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](const boost::system::error_code& ec,
                                   asio::ip::tcp::socket peer) {
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(
                                ec,
                                [&ec,&peer](lua_State* fiber) {
                                    if (ec) {
                                        lua_pushnil(fiber);
                                    } else {
                                        auto s = static_cast<tcp_socket*>(
                                            lua_newuserdata(
                                                fiber, sizeof(tcp_socket)));
                                        rawgetp(fiber, LUA_REGISTRYINDEX,
                                                &ip_tcp_socket_mt_key);
                                        setmetatable(fiber, -2);
                                        new (s) tcp_socket{std::move(peer)};
                                    }
                                }))
                    ));
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(tcp_acceptor)
EMILUA_GPERF_NAMESPACE(emilua)
static int tcp_acceptor_listen(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TNIL: {
        boost::system::error_code ec;
        acceptor->listen(asio::socket_base::max_listen_connections, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TNUMBER: {
        boost::system::error_code ec;
        acceptor->listen(lua_tointeger(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_acceptor_bind(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        acceptor->bind(asio::ip::tcp::endpoint(*addr, lua_tointeger(L, 3)), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        boost::system::error_code ec;
        auto addr = asio::ip::make_address(lua_tostring(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        acceptor->bind(asio::ip::tcp::endpoint(addr, lua_tointeger(L, 3)), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_acceptor_open(lua_State* L)
{
    lua_settop(L, 2);
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        acceptor->open(asio::ip::tcp::endpoint{*addr, 0}.protocol(), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        auto key = tostringview(L, 2);
        auto protocol = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(asio::ip::tcp action)
            EMILUA_GPERF_PAIR("v4", asio::ip::tcp::v4())
            EMILUA_GPERF_PAIR("v6", asio::ip::tcp::v6())
        EMILUA_GPERF_END(key);

        if (!protocol) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        acceptor->open(*protocol, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int tcp_acceptor_close(lua_State* L)
{
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int tcp_acceptor_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, asio::ip::tcp::acceptor*))
        EMILUA_GPERF_DEFAULT_VALUE(
            [](lua_State* L, asio::ip::tcp::acceptor*) -> int {
                push(L, std::errc::not_supported);
                return lua_error(L);
            })
        EMILUA_GPERF_PAIR(
            "debug",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::debug o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                acceptor->set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "reuse_address",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::reuse_address o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                acceptor->set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "enable_connection_aborted",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::enable_connection_aborted o(
                    lua_toboolean(L, 3));
                boost::system::error_code ec;
                acceptor->set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "v6_only",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::ip::v6_only o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                acceptor->set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
    EMILUA_GPERF_END(key)(L, acceptor);
}

static int tcp_acceptor_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, asio::ip::tcp::acceptor*))
        EMILUA_GPERF_DEFAULT_VALUE(
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                push(L, std::errc::not_supported);
                return lua_error(L);
            })
        EMILUA_GPERF_PAIR(
            "debug",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                asio::socket_base::debug o;
                boost::system::error_code ec;
                acceptor->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "reuse_address",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                asio::socket_base::reuse_address o;
                boost::system::error_code ec;
                acceptor->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "enable_connection_aborted",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                asio::socket_base::enable_connection_aborted o;
                boost::system::error_code ec;
                acceptor->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "v6_only",
            [](lua_State* L, asio::ip::tcp::acceptor* acceptor) -> int {
                asio::ip::v6_only o;
                boost::system::error_code ec;
                acceptor->get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
    EMILUA_GPERF_END(key)(L, acceptor);
}

static int tcp_acceptor_cancel(lua_State* L)
{
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    acceptor->cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int tcp_acceptor_assign(lua_State* L)
{
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 3));
    if (!handle || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        lua_pushnil(L);
        setmetatable(L, 3);

        boost::system::error_code ec;
        acceptor->assign(
            asio::ip::tcp::endpoint{*addr, 0}.protocol(), *handle, ec);
        assert(!ec); boost::ignore_unused(ec);

        return 0;
    }
    case LUA_TSTRING: {
        auto key = tostringview(L, 2);
        auto protocol = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(asio::ip::tcp action)
            EMILUA_GPERF_PAIR("v4", asio::ip::tcp::v4())
            EMILUA_GPERF_PAIR("v6", asio::ip::tcp::v6())
        EMILUA_GPERF_END(key);

        if (!protocol) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        lua_pushnil(L);
        setmetatable(L, 3);

        boost::system::error_code ec;
        acceptor->assign(*protocol, *handle, ec);
        assert(!ec); boost::ignore_unused(ec);

        return 0;
    }
    }
}

static int tcp_acceptor_release(lua_State* L)
{
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    if (!acceptor || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_tcp_acceptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (acceptor->native_handle() == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::bad_file_descriptor);
        return lua_error(L);
    }

    boost::system::error_code ec;
    int rawfd = acceptor->release(ec);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
            int res = close(rawfd);
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}
#endif // BOOST_OS_UNIX

inline int tcp_acceptor_is_open(lua_State* L)
{
    auto a = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    lua_pushboolean(L, a->is_open());
    return 1;
}

inline int tcp_acceptor_local_address(lua_State* L)
{
    auto acceptor = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = acceptor->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int tcp_acceptor_local_port(lua_State* L)
{
    auto a = static_cast<asio::ip::tcp::acceptor*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = a->local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}
EMILUA_GPERF_DECLS_END(tcp_acceptor)

static int tcp_acceptor_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "accept",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &tcp_acceptor_accept_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "listen",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_listen);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "bind",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_bind);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "open",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_open);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "set_option",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_set_option);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "get_option",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_get_option);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, tcp_acceptor_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, tcp_acceptor_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, tcp_acceptor_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", tcp_acceptor_is_open)
        EMILUA_GPERF_PAIR("local_address", tcp_acceptor_local_address)
        EMILUA_GPERF_PAIR("local_port", tcp_acceptor_local_port)
    EMILUA_GPERF_END(key)(L);
}

static int tcp_get_address_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::address_configured |
            asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *static_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    ADDRINFOEXW hints;
    hints.ai_flags = flags;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_blob = nullptr;
    hints.ai_bloblen = 0;
    hints.ai_provider = nullptr;
    hints.ai_next = nullptr;

    auto query_ctx = std::make_shared<get_address_info_context_t>(
        vm_ctx->strand().context());

    {
        HANDLE hCompletion = CreateEventA(/*lpEventAttributes=*/NULL,
                                          /*bManualReset=*/TRUE,
                                          /*bInitialState=*/FALSE,
                                          /*lpName=*/NULL);
        if (hCompletion == NULL) {
            boost::system::error_code ec(GetLastError(),
                                         asio::error::get_system_category());
            push(L, ec);
            return lua_error(L);
        }

        boost::system::error_code ec;
        query_ctx->hCompletion.assign(hCompletion, ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        query_ctx->overlapped.hEvent = hCompletion;
    }

    auto service = tostringview(L, 2);
    INT error = GetAddrInfoExW(nowide::widen(host).c_str(),
                               nowide::widen(service).c_str(), NS_DNS,
                               /*lpNspId=*/NULL, &hints, &query_ctx->results,
                               /*timeout=*/NULL, &query_ctx->overlapped,
                               /*lpCompletionRoutine=*/NULL,
                               &query_ctx->hCancel);
    if (error != WSA_IO_PENDING) {
        // the operation completed immediately
        boost::system::error_code ec(WSAGetLastError(),
                                     asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushlightuserdata(L, query_ctx.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto query_ctx = static_cast<get_address_info_context_t*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            GetAddrInfoExCancel(&query_ctx->hCancel);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*query_ctx);
    query_ctx->hCompletion.async_wait(
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,query_ctx](
                boost::system::error_code ec
            ) {
                BOOST_SCOPE_EXIT_ALL(&) {
                    if (query_ctx->results) {
                        FreeAddrInfoExW(query_ctx->results);
#ifndef NDEBUG
                        query_ctx->results = nullptr;
#endif // !defined(NDEBUG)
                    }
                };

                if (vm_ctx->valid()) {
                    vm_ctx->pending_operations.erase(
                        vm_ctx->pending_operations.iterator_to(*query_ctx));
                }

                if (!ec) {
                    INT error = GetAddrInfoExOverlappedResult(
                        &query_ctx->overlapped);
                    if (error == WSA_E_CANCELLED) {
                        ec = asio::error::operation_aborted;
                    } else {
                        ec = boost::system::error_code(
                            error, asio::error::get_system_category());
                    }
                }

                auto push_results = [&ec,&query_ctx](lua_State* L) {
                    if (ec) {
                        lua_pushnil(L);
                        return;
                    }

                    lua_newtable(L);
                    lua_pushliteral(L, "address");
                    lua_pushliteral(L, "port");
                    lua_pushliteral(L, "canonical_name");

                    if (
                        query_ctx->results && query_ctx->results->ai_canonname
                    ) {
                        std::wstring_view w{query_ctx->results->ai_canonname};
                        push(L, nowide::narrow(w));
                    } else {
                        lua_pushnil(L);
                    }

                    int i = 1;
                    for (
                        auto it = query_ctx->results ; it ; it = it->ai_next
                    ) {
                        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

                        asio::ip::tcp::endpoint ep;
                        ep.resize(it->ai_addrlen);
                        std::memcpy(ep.data(), it->ai_addr, ep.size());

                        lua_pushvalue(L, -1 -4);
                        auto a = static_cast<asio::ip::address*>(
                            lua_newuserdata(L, sizeof(asio::ip::address))
                        );
                        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                        setmetatable(L, -2);
                        new (a) asio::ip::address{ep.address()};
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -3);
                        lua_pushinteger(L, ep.port());
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -2);
                        lua_pushvalue(L, -1 -1);
                        lua_rawset(L, -3);

                        lua_rawseti(L, -6, i++);
                    }
                    lua_pop(L, 4);
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))));
            }));
#else // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    bool has_cname = (flags & asio::ip::resolver_base::canonical_name) ?
        true : false;

    service->tcp_resolver.async_resolve(
        host,
        tostringview(L, 2),
        static_cast<asio::ip::tcp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,has_cname](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results,has_cname](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "canonical_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/3);

                            lua_pushvalue(fib, -1 -3);
                            auto a = static_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            if (has_cname) {
                                lua_pushvalue(fib, -1 -1);
                                push(fib, res.host_name());
                                lua_rawset(fib, -3);
                            }

                            lua_rawseti(fib, -5, i++);
                        }
                        lua_pop(fib, 3);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

    return lua_yield(L, 0);
}

static int tcp_get_address_v4_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *static_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    ADDRINFOEXW hints;
    hints.ai_flags = flags;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_blob = nullptr;
    hints.ai_bloblen = 0;
    hints.ai_provider = nullptr;
    hints.ai_next = nullptr;

    auto query_ctx = std::make_shared<get_address_info_context_t>(
        vm_ctx->strand().context());

    {
        HANDLE hCompletion = CreateEventA(/*lpEventAttributes=*/NULL,
                                          /*bManualReset=*/TRUE,
                                          /*bInitialState=*/FALSE,
                                          /*lpName=*/NULL);
        if (hCompletion == NULL) {
            boost::system::error_code ec(GetLastError(),
                                         asio::error::get_system_category());
            push(L, ec);
            return lua_error(L);
        }

        boost::system::error_code ec;
        query_ctx->hCompletion.assign(hCompletion, ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        query_ctx->overlapped.hEvent = hCompletion;
    }

    auto service = tostringview(L, 2);
    INT error = GetAddrInfoExW(nowide::widen(host).c_str(),
                               nowide::widen(service).c_str(), NS_DNS,
                               /*lpNspId=*/NULL, &hints, &query_ctx->results,
                               /*timeout=*/NULL, &query_ctx->overlapped,
                               /*lpCompletionRoutine=*/NULL,
                               &query_ctx->hCancel);
    if (error != WSA_IO_PENDING) {
        // the operation completed immediately
        boost::system::error_code ec(WSAGetLastError(),
                                     asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushlightuserdata(L, query_ctx.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto query_ctx = static_cast<get_address_info_context_t*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            GetAddrInfoExCancel(&query_ctx->hCancel);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*query_ctx);
    query_ctx->hCompletion.async_wait(
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,query_ctx](
                boost::system::error_code ec
            ) {
                BOOST_SCOPE_EXIT_ALL(&) {
                    if (query_ctx->results) {
                        FreeAddrInfoExW(query_ctx->results);
#ifndef NDEBUG
                        query_ctx->results = nullptr;
#endif // !defined(NDEBUG)
                    }
                };

                if (vm_ctx->valid()) {
                    vm_ctx->pending_operations.erase(
                        vm_ctx->pending_operations.iterator_to(*query_ctx));
                }

                if (!ec) {
                    INT error = GetAddrInfoExOverlappedResult(
                        &query_ctx->overlapped);
                    if (error == WSA_E_CANCELLED) {
                        ec = asio::error::operation_aborted;
                    } else {
                        ec = boost::system::error_code(
                            error, asio::error::get_system_category());
                    }
                }

                auto push_results = [&ec,&query_ctx](lua_State* L) {
                    if (ec) {
                        lua_pushnil(L);
                        return;
                    }

                    lua_newtable(L);
                    lua_pushliteral(L, "address");
                    lua_pushliteral(L, "port");
                    lua_pushliteral(L, "canonical_name");

                    if (
                        query_ctx->results && query_ctx->results->ai_canonname
                    ) {
                        std::wstring_view w{query_ctx->results->ai_canonname};
                        push(L, nowide::narrow(w));
                    } else {
                        lua_pushnil(L);
                    }

                    int i = 1;
                    for (
                        auto it = query_ctx->results ; it ; it = it->ai_next
                    ) {
                        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

                        asio::ip::tcp::endpoint ep;
                        ep.resize(it->ai_addrlen);
                        std::memcpy(ep.data(), it->ai_addr, ep.size());

                        lua_pushvalue(L, -1 -4);
                        auto a = static_cast<asio::ip::address*>(
                            lua_newuserdata(L, sizeof(asio::ip::address))
                        );
                        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                        setmetatable(L, -2);
                        new (a) asio::ip::address{ep.address()};
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -3);
                        lua_pushinteger(L, ep.port());
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -2);
                        lua_pushvalue(L, -1 -1);
                        lua_rawset(L, -3);

                        lua_rawseti(L, -6, i++);
                    }
                    lua_pop(L, 4);
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))));
            }));
#else // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    bool has_cname = (flags & asio::ip::resolver_base::canonical_name) ?
        true : false;

    service->tcp_resolver.async_resolve(
        asio::ip::tcp::v4(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::tcp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,has_cname](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results,has_cname](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "canonical_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/3);

                            lua_pushvalue(fib, -1 -3);
                            auto a = static_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            if (has_cname) {
                                lua_pushvalue(fib, -1 -1);
                                push(fib, res.host_name());
                                lua_rawset(fib, -3);
                            }

                            lua_rawseti(fib, -5, i++);
                        }
                        lua_pop(fib, 3);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

    return lua_yield(L, 0);
}

static int tcp_get_address_v6_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *static_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }


#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    ADDRINFOEXW hints;
    hints.ai_flags = flags;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_blob = nullptr;
    hints.ai_bloblen = 0;
    hints.ai_provider = nullptr;
    hints.ai_next = nullptr;

    auto query_ctx = std::make_shared<get_address_info_context_t>(
        vm_ctx->strand().context());

    {
        HANDLE hCompletion = CreateEventA(/*lpEventAttributes=*/NULL,
                                          /*bManualReset=*/TRUE,
                                          /*bInitialState=*/FALSE,
                                          /*lpName=*/NULL);
        if (hCompletion == NULL) {
            boost::system::error_code ec(GetLastError(),
                                         asio::error::get_system_category());
            push(L, ec);
            return lua_error(L);
        }

        boost::system::error_code ec;
        query_ctx->hCompletion.assign(hCompletion, ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        query_ctx->overlapped.hEvent = hCompletion;
    }

    auto service = tostringview(L, 2);
    INT error = GetAddrInfoExW(nowide::widen(host).c_str(),
                               nowide::widen(service).c_str(), NS_DNS,
                               /*lpNspId=*/NULL, &hints, &query_ctx->results,
                               /*timeout=*/NULL, &query_ctx->overlapped,
                               /*lpCompletionRoutine=*/NULL,
                               &query_ctx->hCancel);
    if (error != WSA_IO_PENDING) {
        // the operation completed immediately
        boost::system::error_code ec(WSAGetLastError(),
                                     asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushlightuserdata(L, query_ctx.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto query_ctx = static_cast<get_address_info_context_t*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            GetAddrInfoExCancel(&query_ctx->hCancel);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*query_ctx);
    query_ctx->hCompletion.async_wait(
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,query_ctx](
                boost::system::error_code ec
            ) {
                BOOST_SCOPE_EXIT_ALL(&) {
                    if (query_ctx->results) {
                        FreeAddrInfoExW(query_ctx->results);
#ifndef NDEBUG
                        query_ctx->results = nullptr;
#endif // !defined(NDEBUG)
                    }
                };

                if (vm_ctx->valid()) {
                    vm_ctx->pending_operations.erase(
                        vm_ctx->pending_operations.iterator_to(*query_ctx));
                }

                if (!ec) {
                    INT error = GetAddrInfoExOverlappedResult(
                        &query_ctx->overlapped);
                    if (error == WSA_E_CANCELLED) {
                        ec = asio::error::operation_aborted;
                    } else {
                        ec = boost::system::error_code(
                            error, asio::error::get_system_category());
                    }
                }

                auto push_results = [&ec,&query_ctx](lua_State* L) {
                    if (ec) {
                        lua_pushnil(L);
                        return;
                    }

                    lua_newtable(L);
                    lua_pushliteral(L, "address");
                    lua_pushliteral(L, "port");
                    lua_pushliteral(L, "canonical_name");

                    if (
                        query_ctx->results && query_ctx->results->ai_canonname
                    ) {
                        std::wstring_view w{query_ctx->results->ai_canonname};
                        push(L, nowide::narrow(w));
                    } else {
                        lua_pushnil(L);
                    }

                    int i = 1;
                    for (
                        auto it = query_ctx->results ; it ; it = it->ai_next
                    ) {
                        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

                        asio::ip::tcp::endpoint ep;
                        ep.resize(it->ai_addrlen);
                        std::memcpy(ep.data(), it->ai_addr, ep.size());

                        lua_pushvalue(L, -1 -4);
                        auto a = static_cast<asio::ip::address*>(
                            lua_newuserdata(L, sizeof(asio::ip::address))
                        );
                        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                        setmetatable(L, -2);
                        new (a) asio::ip::address{ep.address()};
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -3);
                        lua_pushinteger(L, ep.port());
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -2);
                        lua_pushvalue(L, -1 -1);
                        lua_rawset(L, -3);

                        lua_rawseti(L, -6, i++);
                    }
                    lua_pop(L, 4);
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))));
            }));
#else // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    bool has_cname = (flags & asio::ip::resolver_base::canonical_name) ?
        true : false;

    service->tcp_resolver.async_resolve(
        asio::ip::tcp::v6(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::tcp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,has_cname](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results,has_cname](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "canonical_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/3);

                            lua_pushvalue(fib, -1 -3);
                            auto a = static_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            if (has_cname) {
                                lua_pushvalue(fib, -1 -1);
                                push(fib, res.host_name());
                                lua_rawset(fib, -3);
                            }

                            lua_rawseti(fib, -5, i++);
                        }
                        lua_pop(fib, 3);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

    return lua_yield(L, 0);
}

static int tcp_get_name_info(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->tcp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->tcp_resolver.async_resolve(
        asio::ip::tcp::endpoint(*a, lua_tointeger(L, 2)),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::tcp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/2);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -4, i++);
                        }
                        lua_pop(fib, 2);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

static int udp_socket_new(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    auto a = static_cast<udp_socket*>(lua_newuserdata(L, sizeof(udp_socket)));
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    setmetatable(L, -2);
    new (a) tcp_socket{vm_ctx.strand().context()};
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(udp_socket)
EMILUA_GPERF_NAMESPACE(emilua)
static int udp_socket_open(lua_State* L)
{
    lua_settop(L, 2);
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        sock->socket.open(asio::ip::udp::endpoint{*addr, 0}.protocol(), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        auto key = tostringview(L, 2);
        auto protocol = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(asio::ip::udp action)
            EMILUA_GPERF_PAIR("v4", asio::ip::udp::v4())
            EMILUA_GPERF_PAIR("v6", asio::ip::udp::v6())
        EMILUA_GPERF_END(key);

        if (!protocol) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        boost::system::error_code ec;
        sock->socket.open(*protocol, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int udp_socket_bind(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        asio::ip::udp::endpoint ep(*addr, lua_tointeger(L, 3));
        boost::system::error_code ec;
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    case LUA_TSTRING: {
        boost::system::error_code ec;
        auto addr = asio::ip::make_address(lua_tostring(L, 2), ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        asio::ip::udp::endpoint ep(addr, lua_tointeger(L, 3));
        sock->socket.bind(ep, ec);
        if (ec) {
            push(L, static_cast<std::error_code>(ec));
            return lua_error(L);
        }
        return 0;
    }
    }
}

static int udp_socket_shutdown(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    auto what = EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(asio::ip::udp::socket::shutdown_type action)
        EMILUA_GPERF_PAIR("receive", asio::ip::udp::socket::shutdown_receive)
        EMILUA_GPERF_PAIR("send", asio::ip::udp::socket::shutdown_send)
        EMILUA_GPERF_PAIR("both", asio::ip::udp::socket::shutdown_both)
    EMILUA_GPERF_END(key);
    if (!what) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    boost::system::error_code ec;
    sock->socket.shutdown(*what, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int udp_socket_disconnect(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

#if BOOST_OS_WINDOWS
    push(L, std::errc::function_not_supported);
    return lua_error(L);
#else
    struct sockaddr_in sin;
    std::memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_UNSPEC;
    int res = connect(
        sock->socket.native_handle(), reinterpret_cast<struct sockaddr*>(&sin),
        sizeof(sin));
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 0;
#endif // BOOST_OS_WINDOWS
}
EMILUA_GPERF_DECLS_END(udp_socket)

static int udp_socket_connect(lua_State* L)
{
    luaL_checktype(L, 3, LUA_TNUMBER);
    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto s = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!s || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
    if (!addr || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    asio::ip::udp::endpoint ep(*addr, lua_tointeger(L, 3));

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++s->nbusy;
    s->socket.async_connect(ep, asio::bind_cancellation_slot(cancel_slot,
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,s](const boost::system::error_code& ec) {
                if (!vm_ctx->valid())
                    return;

                --s->nbusy;

                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(udp_socket)
EMILUA_GPERF_NAMESPACE(emilua)
static int udp_socket_close(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.close(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

static int udp_socket_cancel(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    sock->socket.cancel(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int udp_socket_assign(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto handle = static_cast<file_descriptor_handle*>(lua_touserdata(L, 3));
    if (!handle || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    if (*handle == INVALID_FILE_DESCRIPTOR) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TUSERDATA: {
        auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 2));
        if (!addr || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        lua_pushnil(L);
        setmetatable(L, 3);

        boost::system::error_code ec;
        sock->socket.assign(
            asio::ip::udp::endpoint{*addr, 0}.protocol(), *handle, ec);
        assert(!ec); boost::ignore_unused(ec);

        return 0;
    }
    case LUA_TSTRING: {
        auto key = tostringview(L, 2);
        auto protocol = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(asio::ip::udp action)
            EMILUA_GPERF_PAIR("v4", asio::ip::udp::v4())
            EMILUA_GPERF_PAIR("v6", asio::ip::udp::v6())
        EMILUA_GPERF_END(key);

        if (!protocol) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        lua_pushnil(L);
        setmetatable(L, 3);

        boost::system::error_code ec;
        sock->socket.assign(*protocol, *handle, ec);
        assert(!ec); boost::ignore_unused(ec);

        return 0;
    }
    }
}

static int udp_socket_release(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    boost::system::error_code ec;
    int rawfd = sock->socket.release(ec);
    BOOST_SCOPE_EXIT_ALL(&) {
        if (rawfd != INVALID_FILE_DESCRIPTOR) {
            int res = close(rawfd);
            boost::ignore_unused(res);
        }
    };

    if (ec) {
        push(L, ec);
        return lua_error(L);
    }

    auto fdhandle = static_cast<file_descriptor_handle*>(
        lua_newuserdata(L, sizeof(file_descriptor_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    setmetatable(L, -2);

    *fdhandle = rawfd;
    rawfd = INVALID_FILE_DESCRIPTOR;
    return 1;
}
#endif // BOOST_OS_UNIX

static int udp_socket_set_option(lua_State* L)
{
    lua_settop(L, 3);
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, udp_socket*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, udp_socket*) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "debug",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::debug o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "broadcast",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::broadcast o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "do_not_route",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::do_not_route o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "send_buffer_size",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::socket_base::send_buffer_size o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "receive_buffer_size",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::socket_base::receive_buffer_size o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "reuse_address",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::socket_base::reuse_address o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "multicast_loop",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::ip::multicast::enable_loopback o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "multicast_hops",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::ip::multicast::hops o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "join_multicast_group",
            [](lua_State* L, udp_socket* socket) -> int {
                auto addr = static_cast<asio::ip::address*>(
                    lua_touserdata(L, 3));
                if (!addr || !lua_getmetatable(L, 3)) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                if (!lua_rawequal(L, -1, -2)) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                asio::ip::multicast::join_group o(*addr);
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "leave_multicast_group",
            [](lua_State* L, udp_socket* socket) -> int {
                auto addr = static_cast<asio::ip::address*>(
                    lua_touserdata(L, 3));
                if (!addr || !lua_getmetatable(L, 3)) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                if (!lua_rawequal(L, -1, -2)) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                asio::ip::multicast::leave_group o(*addr);
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "multicast_interface",
            [](lua_State* L, udp_socket* socket) -> int {
                auto addr = static_cast<asio::ip::address*>(
                    lua_touserdata(L, 3));
                if (!addr || !lua_getmetatable(L, 3)) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                if (!lua_rawequal(L, -1, -2)) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                if (!addr->is_v4()) {
                    push(L, std::errc::invalid_argument, "arg", 3);
                    return lua_error(L);
                }
                asio::ip::multicast::outbound_interface o(addr->to_v4());
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "unicast_hops",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TNUMBER);
                asio::ip::unicast::hops o(lua_tointeger(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
        EMILUA_GPERF_PAIR(
            "v6_only",
            [](lua_State* L, udp_socket* socket) -> int {
                luaL_checktype(L, 3, LUA_TBOOLEAN);
                asio::ip::v6_only o(lua_toboolean(L, 3));
                boost::system::error_code ec;
                socket->socket.set_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                return 0;
            })
    EMILUA_GPERF_END(key)(L, socket);
}

static int udp_socket_get_option(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, udp_socket*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, udp_socket* socket) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "debug",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::debug o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "broadcast",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::broadcast o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "do_not_route",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::do_not_route o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_buffer_size",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::send_buffer_size o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "receive_buffer_size",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::receive_buffer_size o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "reuse_address",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::reuse_address o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "multicast_loop",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::ip::multicast::enable_loopback o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "multicast_hops",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::ip::multicast::hops o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "unicast_hops",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::ip::unicast::hops o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushinteger(L, o.value());
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "v6_only",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::ip::v6_only o;
                boost::system::error_code ec;
                socket->socket.get_option(o, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushboolean(L, o.value());
                return 1;
            })
    EMILUA_GPERF_END(key)(L, socket);
}
EMILUA_GPERF_DECLS_END(udp_socket)

static int udp_socket_receive(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
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

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_receive(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int udp_socket_receive_from(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
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

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    auto remote_sender = std::make_shared<asio::ip::udp::endpoint>();

    ++sock->nbusy;
    sock->socket.async_receive_from(
        asio::buffer(bs->data.get(), bs->size),
        *remote_sender,
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,remote_sender,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                auto addr_pusher = [&remote_sender](lua_State* L) {
                    auto a = static_cast<asio::ip::address*>(
                        lua_newuserdata(L, sizeof(asio::ip::address))
                    );
                    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                    setmetatable(L, -2);
                    new (a) asio::ip::address{remote_sender->address()};
                    return 1;
                };
                auto port = remote_sender->port();

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(
                                ec, bytes_transferred, addr_pusher, port))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int udp_socket_send(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
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

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send(
        asio::buffer(bs->data.get(), bs->size),
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int udp_socket_send_to(lua_State* L)
{
    lua_settop(L, 5);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!sock || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
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

    auto addr = static_cast<asio::ip::address*>(lua_touserdata(L, 3));
    if (!addr || !lua_getmetatable(L, 3)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    std::uint16_t port;
    if (lua_type(L, 4) != LUA_TNUMBER) {
        push(L, std::errc::invalid_argument, "arg", 4);
        return lua_error(L);
    }
    port = lua_tointeger(L, 4);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 5)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 5);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 5);
    }

    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    ++sock->nbusy;
    sock->socket.async_send_to(
        asio::buffer(bs->data.get(), bs->size),
        asio::ip::udp::endpoint{*addr, port},
        flags,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,buf=bs->data,sock](
                const boost::system::error_code& ec,
                std::size_t bytes_transferred
            ) {
                boost::ignore_unused(buf);
                if (!vm_ctx->valid())
                    return;

                --sock->nbusy;

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, bytes_transferred))));
            }
        ))
    );

    return lua_yield(L, 0);
}

EMILUA_GPERF_DECLS_BEGIN(udp_socket)
EMILUA_GPERF_NAMESPACE(emilua)
static int udp_socket_io_control(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TSTRING);

    auto socket = static_cast<udp_socket*>(lua_touserdata(L, 1));
    if (!socket || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_udp_socket_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*, udp_socket*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L, udp_socket*) -> int {
            push(L, std::errc::not_supported);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "bytes_readable",
            [](lua_State* L, udp_socket* socket) -> int {
                asio::socket_base::bytes_readable command;
                boost::system::error_code ec;
                socket->socket.io_control(command, ec);
                if (ec) {
                    push(L, static_cast<std::error_code>(ec));
                    return lua_error(L);
                }
                lua_pushnumber(L, command.get());
                return 1;
            })
    EMILUA_GPERF_END(key)(L, socket);
}

inline int udp_socket_is_open(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    lua_pushboolean(L, sock->socket.is_open());
    return 1;
}

inline int udp_socket_local_address(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int udp_socket_local_port(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.local_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}

inline int udp_socket_remote_address(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    auto addr = static_cast<asio::ip::address*>(
        lua_newuserdata(L, sizeof(asio::ip::address))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    setmetatable(L, -2);
    new (addr) asio::ip::address{ep.address()};
    return 1;
}

inline int udp_socket_remote_port(lua_State* L)
{
    auto sock = static_cast<udp_socket*>(lua_touserdata(L, 1));
    boost::system::error_code ec;
    auto ep = sock->socket.remote_endpoint(ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }
    lua_pushinteger(L, ep.port());
    return 1;
}
EMILUA_GPERF_DECLS_END(udp_socket)

static int udp_socket_mt_index(lua_State* L)
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
                lua_pushcfunction(L, udp_socket_open);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "bind",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_bind);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "shutdown",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_shutdown);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "disconnect",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_disconnect);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "connect",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_connect_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "close",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_close);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cancel",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_cancel);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "assign",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, udp_socket_assign);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "release",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, udp_socket_release);
#else // BOOST_OS_UNIX
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "set_option",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_set_option);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "get_option",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_get_option);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "receive",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_receive_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "receive_from",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_receive_from_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_send_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_to",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &udp_socket_send_to_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "io_control",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, udp_socket_io_control);
                return 1;
            })
        EMILUA_GPERF_PAIR("is_open", udp_socket_is_open)
        EMILUA_GPERF_PAIR("local_address", udp_socket_local_address)
        EMILUA_GPERF_PAIR("local_port", udp_socket_local_port)
        EMILUA_GPERF_PAIR("remote_address", udp_socket_remote_address)
        EMILUA_GPERF_PAIR("remote_port", udp_socket_remote_port)
    EMILUA_GPERF_END(key)(L);
}

static int udp_get_address_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::address_configured |
            asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *static_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    ADDRINFOEXW hints;
    hints.ai_flags = flags;
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_blob = nullptr;
    hints.ai_bloblen = 0;
    hints.ai_provider = nullptr;
    hints.ai_next = nullptr;

    auto query_ctx = std::make_shared<get_address_info_context_t>(
        vm_ctx->strand().context());

    {
        HANDLE hCompletion = CreateEventA(/*lpEventAttributes=*/NULL,
                                          /*bManualReset=*/TRUE,
                                          /*bInitialState=*/FALSE,
                                          /*lpName=*/NULL);
        if (hCompletion == NULL) {
            boost::system::error_code ec(GetLastError(),
                                         asio::error::get_system_category());
            push(L, ec);
            return lua_error(L);
        }

        boost::system::error_code ec;
        query_ctx->hCompletion.assign(hCompletion, ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        query_ctx->overlapped.hEvent = hCompletion;
    }

    auto service = tostringview(L, 2);
    INT error = GetAddrInfoExW(nowide::widen(host).c_str(),
                               nowide::widen(service).c_str(), NS_DNS,
                               /*lpNspId=*/NULL, &hints, &query_ctx->results,
                               /*timeout=*/NULL, &query_ctx->overlapped,
                               /*lpCompletionRoutine=*/NULL,
                               &query_ctx->hCancel);
    if (error != WSA_IO_PENDING) {
        // the operation completed immediately
        boost::system::error_code ec(WSAGetLastError(),
                                     asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushlightuserdata(L, query_ctx.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto query_ctx = static_cast<get_address_info_context_t*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            GetAddrInfoExCancel(&query_ctx->hCancel);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*query_ctx);
    query_ctx->hCompletion.async_wait(
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,query_ctx](
                boost::system::error_code ec
            ) {
                BOOST_SCOPE_EXIT_ALL(&) {
                    if (query_ctx->results) {
                        FreeAddrInfoExW(query_ctx->results);
#ifndef NDEBUG
                        query_ctx->results = nullptr;
#endif // !defined(NDEBUG)
                    }
                };

                if (vm_ctx->valid()) {
                    vm_ctx->pending_operations.erase(
                        vm_ctx->pending_operations.iterator_to(*query_ctx));
                }

                if (!ec) {
                    INT error = GetAddrInfoExOverlappedResult(
                        &query_ctx->overlapped);
                    if (error == WSA_E_CANCELLED) {
                        ec = asio::error::operation_aborted;
                    } else {
                        ec = boost::system::error_code(
                            error, asio::error::get_system_category());
                    }
                }

                auto push_results = [&ec,&query_ctx](lua_State* L) {
                    if (ec) {
                        lua_pushnil(L);
                        return;
                    }

                    lua_newtable(L);
                    lua_pushliteral(L, "address");
                    lua_pushliteral(L, "port");
                    lua_pushliteral(L, "canonical_name");

                    if (
                        query_ctx->results && query_ctx->results->ai_canonname
                    ) {
                        std::wstring_view w{query_ctx->results->ai_canonname};
                        push(L, nowide::narrow(w));
                    } else {
                        lua_pushnil(L);
                    }

                    int i = 1;
                    for (
                        auto it = query_ctx->results ; it ; it = it->ai_next
                    ) {
                        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

                        asio::ip::tcp::endpoint ep;
                        ep.resize(it->ai_addrlen);
                        std::memcpy(ep.data(), it->ai_addr, ep.size());

                        lua_pushvalue(L, -1 -4);
                        auto a = static_cast<asio::ip::address*>(
                            lua_newuserdata(L, sizeof(asio::ip::address))
                        );
                        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                        setmetatable(L, -2);
                        new (a) asio::ip::address{ep.address()};
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -3);
                        lua_pushinteger(L, ep.port());
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -2);
                        lua_pushvalue(L, -1 -1);
                        lua_rawset(L, -3);

                        lua_rawseti(L, -6, i++);
                    }
                    lua_pop(L, 4);
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))));
            }));
#else // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    bool has_cname = (flags & asio::ip::resolver_base::canonical_name) ?
        true : false;

    service->udp_resolver.async_resolve(
        host,
        tostringview(L, 2),
        static_cast<asio::ip::udp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,has_cname](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results,has_cname](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "canonical_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/3);

                            lua_pushvalue(fib, -1 -3);
                            auto a = static_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            if (has_cname) {
                                lua_pushvalue(fib, -1 -1);
                                push(fib, res.host_name());
                                lua_rawset(fib, -3);
                            }

                            lua_rawseti(fib, -5, i++);
                        }
                        lua_pop(fib, 3);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

    return lua_yield(L, 0);
}

static int udp_get_address_v4_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = 0;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *static_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    ADDRINFOEXW hints;
    hints.ai_flags = flags;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_blob = nullptr;
    hints.ai_bloblen = 0;
    hints.ai_provider = nullptr;
    hints.ai_next = nullptr;

    auto query_ctx = std::make_shared<get_address_info_context_t>(
        vm_ctx->strand().context());

    {
        HANDLE hCompletion = CreateEventA(/*lpEventAttributes=*/NULL,
                                          /*bManualReset=*/TRUE,
                                          /*bInitialState=*/FALSE,
                                          /*lpName=*/NULL);
        if (hCompletion == NULL) {
            boost::system::error_code ec(GetLastError(),
                                         asio::error::get_system_category());
            push(L, ec);
            return lua_error(L);
        }

        boost::system::error_code ec;
        query_ctx->hCompletion.assign(hCompletion, ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        query_ctx->overlapped.hEvent = hCompletion;
    }

    auto service = tostringview(L, 2);
    INT error = GetAddrInfoExW(nowide::widen(host).c_str(),
                               nowide::widen(service).c_str(), NS_DNS,
                               /*lpNspId=*/NULL, &hints, &query_ctx->results,
                               /*timeout=*/NULL, &query_ctx->overlapped,
                               /*lpCompletionRoutine=*/NULL,
                               &query_ctx->hCancel);
    if (error != WSA_IO_PENDING) {
        // the operation completed immediately
        boost::system::error_code ec(WSAGetLastError(),
                                     asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushlightuserdata(L, query_ctx.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto query_ctx = static_cast<get_address_info_context_t*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            GetAddrInfoExCancel(&query_ctx->hCancel);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*query_ctx);
    query_ctx->hCompletion.async_wait(
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,query_ctx](
                boost::system::error_code ec
            ) {
                BOOST_SCOPE_EXIT_ALL(&) {
                    if (query_ctx->results) {
                        FreeAddrInfoExW(query_ctx->results);
#ifndef NDEBUG
                        query_ctx->results = nullptr;
#endif // !defined(NDEBUG)
                    }
                };

                if (vm_ctx->valid()) {
                    vm_ctx->pending_operations.erase(
                        vm_ctx->pending_operations.iterator_to(*query_ctx));
                }

                if (!ec) {
                    INT error = GetAddrInfoExOverlappedResult(
                        &query_ctx->overlapped);
                    if (error == WSA_E_CANCELLED) {
                        ec = asio::error::operation_aborted;
                    } else {
                        ec = boost::system::error_code(
                            error, asio::error::get_system_category());
                    }
                }

                auto push_results = [&ec,&query_ctx](lua_State* L) {
                    if (ec) {
                        lua_pushnil(L);
                        return;
                    }

                    lua_newtable(L);
                    lua_pushliteral(L, "address");
                    lua_pushliteral(L, "port");
                    lua_pushliteral(L, "canonical_name");

                    if (
                        query_ctx->results && query_ctx->results->ai_canonname
                    ) {
                        std::wstring_view w{query_ctx->results->ai_canonname};
                        push(L, nowide::narrow(w));
                    } else {
                        lua_pushnil(L);
                    }

                    int i = 1;
                    for (
                        auto it = query_ctx->results ; it ; it = it->ai_next
                    ) {
                        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

                        asio::ip::tcp::endpoint ep;
                        ep.resize(it->ai_addrlen);
                        std::memcpy(ep.data(), it->ai_addr, ep.size());

                        lua_pushvalue(L, -1 -4);
                        auto a = static_cast<asio::ip::address*>(
                            lua_newuserdata(L, sizeof(asio::ip::address))
                        );
                        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                        setmetatable(L, -2);
                        new (a) asio::ip::address{ep.address()};
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -3);
                        lua_pushinteger(L, ep.port());
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -2);
                        lua_pushvalue(L, -1 -1);
                        lua_rawset(L, -3);

                        lua_rawseti(L, -6, i++);
                    }
                    lua_pop(L, 4);
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))));
            }));
#else // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    bool has_cname = (flags & asio::ip::resolver_base::canonical_name) ?
        true : false;

    service->udp_resolver.async_resolve(
        asio::ip::udp::v4(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::udp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,has_cname](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results,has_cname](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "canonical_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/3);

                            lua_pushvalue(fib, -1 -3);
                            auto a = static_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            if (has_cname) {
                                lua_pushvalue(fib, -1 -1);
                                push(fib, res.host_name());
                                lua_rawset(fib, -3);
                            }

                            lua_rawseti(fib, -5, i++);
                        }
                        lua_pop(fib, 3);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

    return lua_yield(L, 0);
}

static int udp_get_address_v6_info(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    // Lua BitOp underlying type is int32
    std::int32_t flags;
    switch (lua_type(L, 3)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    case LUA_TNIL:
        flags = asio::ip::resolver_base::v4_mapped;
        break;
    case LUA_TNUMBER:
        flags = lua_tointeger(L, 3);
    }

    std::string host;
    switch (lua_type(L, 1)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    case LUA_TSTRING:
        host = tostringview(L, 1);
        break;
    case LUA_TUSERDATA: {
        if (!lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        auto& a = *static_cast<asio::ip::address*>(lua_touserdata(L, 1));
        host = a.to_string();
        flags |= asio::ip::resolver_base::numeric_host;
    }
    }

    switch (lua_type(L, 2)) {
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    case LUA_TSTRING:
        break;
    case LUA_TNUMBER:
        flags |= asio::ip::resolver_base::numeric_service;
    }

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    ADDRINFOEXW hints;
    hints.ai_flags = flags;
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_addrlen = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr = nullptr;
    hints.ai_blob = nullptr;
    hints.ai_bloblen = 0;
    hints.ai_provider = nullptr;
    hints.ai_next = nullptr;

    auto query_ctx = std::make_shared<get_address_info_context_t>(
        vm_ctx->strand().context());

    {
        HANDLE hCompletion = CreateEventA(/*lpEventAttributes=*/NULL,
                                          /*bManualReset=*/TRUE,
                                          /*bInitialState=*/FALSE,
                                          /*lpName=*/NULL);
        if (hCompletion == NULL) {
            boost::system::error_code ec(GetLastError(),
                                         asio::error::get_system_category());
            push(L, ec);
            return lua_error(L);
        }

        boost::system::error_code ec;
        query_ctx->hCompletion.assign(hCompletion, ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        query_ctx->overlapped.hEvent = hCompletion;
    }

    auto service = tostringview(L, 2);
    INT error = GetAddrInfoExW(nowide::widen(host).c_str(),
                               nowide::widen(service).c_str(), NS_DNS,
                               /*lpNspId=*/NULL, &hints, &query_ctx->results,
                               /*timeout=*/NULL, &query_ctx->overlapped,
                               /*lpCompletionRoutine=*/NULL,
                               &query_ctx->hCancel);
    if (error != WSA_IO_PENDING) {
        // the operation completed immediately
        boost::system::error_code ec(WSAGetLastError(),
                                     asio::error::get_system_category());
        push(L, ec);
        return lua_error(L);
    }

    lua_pushlightuserdata(L, query_ctx.get());
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto query_ctx = static_cast<get_address_info_context_t*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            GetAddrInfoExCancel(&query_ctx->hCancel);
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    vm_ctx->pending_operations.push_back(*query_ctx);
    query_ctx->hCompletion.async_wait(
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,query_ctx](
                boost::system::error_code ec
            ) {
                BOOST_SCOPE_EXIT_ALL(&) {
                    if (query_ctx->results) {
                        FreeAddrInfoExW(query_ctx->results);
#ifndef NDEBUG
                        query_ctx->results = nullptr;
#endif // !defined(NDEBUG)
                    }
                };

                if (vm_ctx->valid()) {
                    vm_ctx->pending_operations.erase(
                        vm_ctx->pending_operations.iterator_to(*query_ctx));
                }

                if (!ec) {
                    INT error = GetAddrInfoExOverlappedResult(
                        &query_ctx->overlapped);
                    if (error == WSA_E_CANCELLED) {
                        ec = asio::error::operation_aborted;
                    } else {
                        ec = boost::system::error_code(
                            error, asio::error::get_system_category());
                    }
                }

                auto push_results = [&ec,&query_ctx](lua_State* L) {
                    if (ec) {
                        lua_pushnil(L);
                        return;
                    }

                    lua_newtable(L);
                    lua_pushliteral(L, "address");
                    lua_pushliteral(L, "port");
                    lua_pushliteral(L, "canonical_name");

                    if (
                        query_ctx->results && query_ctx->results->ai_canonname
                    ) {
                        std::wstring_view w{query_ctx->results->ai_canonname};
                        push(L, nowide::narrow(w));
                    } else {
                        lua_pushnil(L);
                    }

                    int i = 1;
                    for (
                        auto it = query_ctx->results ; it ; it = it->ai_next
                    ) {
                        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

                        asio::ip::tcp::endpoint ep;
                        ep.resize(it->ai_addrlen);
                        std::memcpy(ep.data(), it->ai_addr, ep.size());

                        lua_pushvalue(L, -1 -4);
                        auto a = static_cast<asio::ip::address*>(
                            lua_newuserdata(L, sizeof(asio::ip::address))
                        );
                        rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                        setmetatable(L, -2);
                        new (a) asio::ip::address{ep.address()};
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -3);
                        lua_pushinteger(L, ep.port());
                        lua_rawset(L, -3);

                        lua_pushvalue(L, -1 -2);
                        lua_pushvalue(L, -1 -1);
                        lua_rawset(L, -3);

                        lua_rawseti(L, -6, i++);
                    }
                    lua_pop(L, 4);
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::fast_auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))));
            }));
#else // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)
    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    bool has_cname = (flags & asio::ip::resolver_base::canonical_name) ?
        true : false;

    service->udp_resolver.async_resolve(
        asio::ip::udp::v6(),
        host,
        tostringview(L, 2),
        static_cast<asio::ip::udp::resolver::flags>(flags),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,has_cname](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results,has_cname](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "address");
                        lua_pushliteral(fib, "port");
                        lua_pushliteral(fib, "canonical_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/3);

                            lua_pushvalue(fib, -1 -3);
                            auto a = static_cast<asio::ip::address*>(
                                lua_newuserdata(fib, sizeof(asio::ip::address))
                            );
                            rawgetp(fib, LUA_REGISTRYINDEX, &ip_address_mt_key);
                            setmetatable(fib, -2);
                            new (a) asio::ip::address{res.endpoint().address()};
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -2);
                            lua_pushinteger(fib, res.endpoint().port());
                            lua_rawset(fib, -3);

                            if (has_cname) {
                                lua_pushvalue(fib, -1 -1);
                                push(fib, res.host_name());
                                lua_rawset(fib, -3);
                            }

                            lua_rawseti(fib, -5, i++);
                        }
                        lua_pop(fib, 3);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );
#endif // defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0602)

    return lua_yield(L, 0);
}

static int udp_get_name_info(lua_State* L)
{
    luaL_checktype(L, 2, LUA_TNUMBER);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto a = static_cast<asio::ip::address*>(lua_touserdata(L, 1));
    if (!a || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    resolver_service* service = nullptr;

    for (auto& op: vm_ctx->pending_operations) {
        service = dynamic_cast<resolver_service*>(&op);
        if (service)
            break;
    }

    if (!service) {
        service = new resolver_service{vm_ctx->strand().context()};
        vm_ctx->pending_operations.push_back(*service);
    }

    lua_pushlightuserdata(L, service);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto service = static_cast<resolver_service*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            try {
                service->udp_resolver.cancel();
            } catch (const boost::system::system_error&) {}
            return 0;
        },
        1);
    set_interrupter(L, *vm_ctx);

    service->udp_resolver.async_resolve(
        asio::ip::udp::endpoint(*a, lua_tointeger(L, 2)),
        asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber](
                const boost::system::error_code& ec,
                asio::ip::udp::resolver::results_type results
            ) {
                auto push_results = [&ec,&results](lua_State* fib) {
                    if (ec) {
                        lua_pushnil(fib);
                    } else {
                        lua_createtable(fib, /*narr=*/results.size(),
                                        /*nrec=*/0);
                        lua_pushliteral(fib, "host_name");
                        lua_pushliteral(fib, "service_name");

                        int i = 1;
                        for (const auto& res: results) {
                            lua_createtable(fib, /*narr=*/0, /*nrec=*/2);

                            lua_pushvalue(fib, -1 -2);
                            push(fib, res.host_name());
                            lua_rawset(fib, -3);

                            lua_pushvalue(fib, -1 -1);
                            push(fib, res.service_name());
                            lua_rawset(fib, -3);

                            lua_rawseti(fib, -4, i++);
                        }
                        lua_pop(fib, 2);
                    }
                };

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(
                            vm_context::options::arguments,
                            hana::make_tuple(ec, push_results))
                    ));
            }
        )
    );

    return lua_yield(L, 0);
}

void init_ip(lua_State* L)
{
    lua_pushlightuserdata(L, &ip_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/9);

        lua_pushliteral(L, "tostring");
        lua_pushcfunction(L, ip_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "toendpoint");
        lua_pushcfunction(L, ip_toendpoint);
        lua_rawset(L, -3);

        lua_pushliteral(L, "host_name");
        lua_pushcfunction(L, ip_host_name);
        lua_rawset(L, -3);

        lua_pushliteral(L, "connect");
        int res = luaL_loadbuffer(
            L, reinterpret_cast<char*>(ip_connect_bytecode),
            ip_connect_bytecode_size, nullptr);
        assert(res == 0); boost::ignore_unused(res);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_type_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_next_key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_pcall_key);
        push(L, make_error_code(asio::error::not_found));
        push(L, std::errc::invalid_argument);
        lua_call(L, 6, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "address");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/6);

            lua_pushliteral(L, "new");
            lua_pushcfunction(L, address_new);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any_v4");
            lua_pushcfunction(L, address_any_v4);
            lua_rawset(L, -3);

            lua_pushliteral(L, "any_v6");
            lua_pushcfunction(L, address_any_v6);
            lua_rawset(L, -3);

            lua_pushliteral(L, "loopback_v4");
            lua_pushcfunction(L, address_loopback_v4);
            lua_rawset(L, -3);

            lua_pushliteral(L, "loopback_v6");
            lua_pushcfunction(L, address_loopback_v6);
            lua_rawset(L, -3);

            lua_pushliteral(L, "broadcast_v4");
            lua_pushcfunction(L, address_broadcast_v4);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "message_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/4);

            lua_pushliteral(L, "do_not_route");
            lua_pushinteger(L, asio::socket_base::message_do_not_route);
            lua_rawset(L, -3);

            lua_pushliteral(L, "end_of_record");
            lua_pushinteger(L, asio::socket_base::message_end_of_record);
            lua_rawset(L, -3);

            lua_pushliteral(L, "out_of_band");
            lua_pushinteger(L, asio::socket_base::message_out_of_band);
            lua_rawset(L, -3);

            lua_pushliteral(L, "peek");
            lua_pushinteger(L, asio::socket_base::message_peek);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "address_info_flag");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/7);

            lua_pushliteral(L, "address_configured");
            lua_pushinteger(L, asio::ip::resolver_base::address_configured);
            lua_rawset(L, -3);

            lua_pushliteral(L, "all_matching");
            lua_pushinteger(L, asio::ip::resolver_base::all_matching);
            lua_rawset(L, -3);

            lua_pushliteral(L, "canonical_name");
            lua_pushinteger(L, asio::ip::resolver_base::canonical_name);
            lua_rawset(L, -3);

            lua_pushliteral(L, "passive");
            lua_pushinteger(L, asio::ip::resolver_base::passive);
            lua_rawset(L, -3);

            lua_pushliteral(L, "v4_mapped");
            lua_pushinteger(L, asio::ip::resolver_base::v4_mapped);
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "tcp");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/6);

            lua_pushliteral(L, "socket");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, tcp_socket_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "acceptor");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, tcp_acceptor_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_address_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v4_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_address_v4_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v6_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_address_v6_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_name_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, tcp_get_name_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "udp");
        {
            lua_createtable(L, /*narr=*/0, /*nrec=*/5);

            lua_pushliteral(L, "socket");
            {
                lua_createtable(L, /*narr=*/0, /*nrec=*/1);

                lua_pushliteral(L, "new");
                lua_pushcfunction(L, udp_socket_new);
                lua_rawset(L, -3);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_address_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v4_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_address_v4_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_address_v6_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_address_v6_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);

            lua_pushliteral(L, "get_name_info");
            {
                rawgetp(L, LUA_REGISTRYINDEX,
                        &var_args__retval1_to_error__fwd_retval2__key);
                rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
                lua_pushcfunction(L, udp_get_name_info);
                lua_call(L, 2, 1);
            }
            lua_rawset(L, -3);
        }
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_address_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<asio::ip::address>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.address");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, address_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, address_mt_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, address_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, address_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, address_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, address_mt_le);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_tcp_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.tcp.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tcp_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<tcp_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_tcp_acceptor_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.tcp.acceptor");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, tcp_acceptor_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<asio::ip::tcp::acceptor>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &ip_udp_socket_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "ip.udp.socket");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, udp_socket_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<udp_socket>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_connect_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_connect);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_read_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_read_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_write_some_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_write_some);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_receive_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_socket_send_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

#if BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO
    lua_pushlightuserdata(L, &tcp_socket_send_file_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_send_file);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
#endif // BOOST_OS_WINDOWS && EMILUA_CONFIG_ENABLE_FILE_IO

    lua_pushlightuserdata(L, &tcp_socket_wait_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_socket_wait);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &tcp_acceptor_accept_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, tcp_acceptor_accept);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_connect_key);
    rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_connect);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_receive_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_receive);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_receive_from_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval234__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_receive_from);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_send_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_send);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &udp_socket_send_to_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, udp_socket_send_to);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

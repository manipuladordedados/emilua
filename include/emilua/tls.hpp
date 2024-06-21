// Copyright (c) 2020 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/ssl/context.hpp>
#include <asio/ssl/stream.hpp>
#include <asio/ip/tcp.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ip/tcp.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

#include <emilua/core.hpp>

namespace emilua {

extern char tls_key;
extern char tls_context_mt_key;
extern char tls_socket_mt_key;

class TlsSocket
    : private std::shared_ptr<asio::ssl::context>
    , public asio::ssl::stream<asio::ip::tcp::socket>
{
public:
    TlsSocket(asio::ip::tcp::socket& socket,
              std::shared_ptr<asio::ssl::context> tls_context)
        : std::shared_ptr<asio::ssl::context>{std::move(tls_context)}
        , asio::ssl::stream<asio::ip::tcp::socket>{
            std::move(socket),
            *static_cast<std::shared_ptr<asio::ssl::context>&>(*this)}
    {}

    TlsSocket(TlsSocket&&) = default;

    std::shared_ptr<asio::ssl::context>& tls_context()
    {
        return *this;
    }
};

void init_tls(lua_State* L);

} // namespace emilua

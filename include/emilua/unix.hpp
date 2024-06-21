// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/local/datagram_protocol.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/local/seq_packet_protocol.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/local/datagram_protocol.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/local/seq_packet_protocol.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

#include <emilua/socket_base.hpp>

namespace emilua {

extern char unix_key;
extern char unix_datagram_socket_mt_key;
extern char unix_stream_socket_mt_key;

using unix_datagram_socket = Socket<asio::local::datagram_protocol::socket>;
using unix_stream_socket = Socket<asio::local::stream_protocol::socket>;
using unix_seqpacket_socket = Socket<asio::local::seq_packet_protocol::socket>;

void init_unix(lua_State* L);

} // namespace emilua

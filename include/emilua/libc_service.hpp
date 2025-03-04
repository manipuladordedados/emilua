// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

#if !BOOST_OS_UNIX
# error "libc_service only supported on POSIX-like systems"
#endif // !BOOST_OS_UNIX

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
# include <asio/local/seq_packet_protocol.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
# include <boost/asio/local/seq_packet_protocol.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua::libc_service {

struct slave
{
    slave(asio::io_context& ioctx)
        : socket{ioctx}
    {}

    ~slave()
    {
        if (masterdupfd != -1)
            std::ignore = close(masterdupfd);
    }

    asio::local::seq_packet_protocol::socket socket;
    std::map<int, std::string> lua_chunk_filters;
    int masterdupfd = -1;
};

extern char key;
extern char slave_mt_key;

void init(lua_State* L);

} // namespace emilua::libc_service

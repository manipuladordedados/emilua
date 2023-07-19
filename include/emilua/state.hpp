/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

#include <boost/asio/io_context.hpp>

namespace emilua {

std::shared_ptr<vm_context> make_vm(
    boost::asio::io_context& ioctx,
    emilua::app_context& appctx,
    ContextType lua_context,
    std::filesystem::path entry_point,
    std::filesystem::path import_root = std::filesystem::path{});

} // namespace emilua

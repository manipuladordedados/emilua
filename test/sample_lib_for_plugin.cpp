// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <string>

#include <boost/dll.hpp>

BOOST_SYMBOL_EXPORT std::string foobar()
{
    return "hello from sample lib for plugin";
}

// Copyright (c) 2022 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

namespace emilua {

template<class T>
struct EMILUA_API Socket
{
    template<class... Args>
    Socket(Args&&... args) : socket{std::forward<Args>(args)...} {}

    T socket;
    std::size_t nbusy = 0; //< TODO: use to errcheck transfers between actors
};

} // namespace emilua

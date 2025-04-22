// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <memory>

#include <mimalloc.h>

extern "C" {
#include <lua.h>
}

namespace emilua {

class general_purpose_allocator
{
public:
    general_purpose_allocator(
        std::shared_ptr<void> block = nullptr, std::size_t block_size = 0);
    ~general_purpose_allocator();

    lua_Alloc get_lua_allocator();

    void allow_reserved_zone();

private:
    void* do_mimalloc(void* ptr, std::size_t osize, std::size_t nsize);

    std::shared_ptr<void> block;
    mi_heap_t* heap = nullptr;
    void* reserved_zone = nullptr;
};

} // namespace emilua

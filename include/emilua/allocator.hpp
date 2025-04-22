// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <boost/interprocess/managed_external_buffer.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <optional>
#include <memory>

extern "C" {
#include <lua.h>
}

namespace emilua {

class general_purpose_allocator
{
public:
    general_purpose_allocator(
        std::shared_ptr<void> block = nullptr, std::size_t block_size = 0);

    lua_Alloc get_lua_allocator();

    void allow_reserved_zone();

private:
    struct unit_type
    {
        [[maybe_unused]]
        alignas(std::max_align_t) char block[alignof(std::max_align_t)];
    };

    static std::size_t bytes_to_units(std::size_t bytes);
    void* do_alloc(void* ptr, std::size_t osize, std::size_t nsize);

    std::shared_ptr<void> block;
    boost::interprocess::managed_external_buffer external_buffer_manager;
    std::optional<boost::interprocess::allocator<
        unit_type, boost::interprocess::managed_external_buffer::segment_manager
    >> allocator;
};

} // namespace emilua

// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/allocator.hpp>
#include <cassert>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

// TODO: move to <emilua/config.h>
static constexpr std::size_t reserved_zone_size = 2048;

static inline
void* do_std_alloc(void* ptr, std::size_t osize, std::size_t nsize)
{
    if (nsize == 0) {
        // free_sized only appeared in C23
        free(ptr);
        return nullptr;
    } else {
        // even in C23, we don't have realloc_sized() to pass osize along
        auto ret = realloc(ptr, nsize);
        if (nsize <= osize) {
            // According to Programming in Lua 3rd edition § 32.1 ¶ 8, Lua is
            // unable to recover from allocation shrinking failures.
            assert(ret);
        }
        return ret;
    }
}

general_purpose_allocator::general_purpose_allocator(
    std::shared_ptr<void> block, std::size_t block_size)
    : block{std::move(block)}
{
    if (this->block) {
        assert(block_size > 0);
        mi_arena_id_t arena_id;
        if (!mi_manage_os_memory_ex(
            this->block.get(), block_size, /*is_committed=*/true,
            /*is_large=*/false, /*is_zero=*/false,
            /*numa_node=no_preference*/-1, /*exclusive=*/true, &arena_id)) {
            throw std::bad_alloc{};
        }

        heap = mi_heap_new_in_arena(arena_id);
        if (!heap)
            throw std::bad_alloc{};

        reserved_zone = mi_heap_malloc(heap, reserved_zone_size);
        if (!reserved_zone) {
            mi_heap_delete(heap);
            throw std::bad_alloc{};
        }
    }
}

general_purpose_allocator::~general_purpose_allocator()
{
    if (heap) {
        mi_heap_delete(heap);
    }
}

lua_Alloc general_purpose_allocator::get_lua_allocator()
{
    static constexpr auto use_mimalloc = [](
        void* ud, void* ptr, std::size_t osize, std::size_t nsize
    ) -> void* {
        return static_cast<general_purpose_allocator*>(ud)->do_mimalloc(
            ptr, osize, nsize);
    };

    static constexpr auto use_c_allocator = [](
        void* /*ud*/, void* ptr, std::size_t osize, std::size_t nsize
    ) -> void* {
        return do_std_alloc(ptr, osize, nsize);
    };

    if (heap) {
        return use_mimalloc;
    } else {
        return use_c_allocator;
    }
}

void general_purpose_allocator::allow_reserved_zone()
{
    if (reserved_zone) {
        mi_free_size(reserved_zone, reserved_zone_size);
        reserved_zone = nullptr;
    }
}

void* general_purpose_allocator::do_mimalloc(
    void* ptr, std::size_t osize, std::size_t nsize)
{
    if (nsize == 0) {
        mi_free_size(ptr, osize);
        return nullptr;
    } else {
        auto ret = mi_heap_realloc(heap, ptr, nsize);
        if (nsize <= osize) {
            // According to Programming in Lua 3rd edition § 32.1 ¶ 8, Lua is
            // unable to recover from allocation shrinking failures.
            assert(ret);
        }
        return ret;
    }
}

} // namespace emilua

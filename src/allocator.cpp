// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/allocator.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

namespace interprocess = boost::interprocess;

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
        external_buffer_manager = interprocess::managed_external_buffer{
            interprocess::create_only, this->block.get(), block_size
        };
        allocator.emplace(external_buffer_manager.get_segment_manager());
    }
}

lua_Alloc general_purpose_allocator::get_lua_allocator()
{
    static constexpr auto use_boost_allocator = [](
        void* ud, void* ptr, std::size_t osize, std::size_t nsize
    ) -> void* {
        return static_cast<general_purpose_allocator*>(ud)->do_alloc(
            ptr, osize, nsize);
    };

    static constexpr auto use_c_allocator = [](
        void* /*ud*/, void* ptr, std::size_t osize, std::size_t nsize
    ) -> void* {
        return do_std_alloc(ptr, osize, nsize);
    };

    if (allocator) {
        return use_boost_allocator;
    } else {
        return use_c_allocator;
    }
}

void general_purpose_allocator::allow_reserved_zone()
{
    // TODO: allow allocations to proceed through fallback memory resources if
    // necessary
}

inline std::size_t general_purpose_allocator::bytes_to_units(std::size_t bytes)
{
    return (bytes + sizeof(unit_type) - 1) / sizeof(unit_type);
}

void* general_purpose_allocator::do_alloc(
    void* ptr, std::size_t osize, std::size_t nsize)
{
    if (nsize == 0) {
        if (!allocator) {
            return nullptr;
        }

        allocator->deallocate(
            static_cast<unit_type*>(ptr), bytes_to_units(osize));
        return nullptr;
    } else if (ptr == nullptr) { // malloc
        if (!allocator) {
            return nullptr;
        }

        try {
            return allocator->allocate(bytes_to_units(nsize)).get();
        } catch (const interprocess::bad_alloc&) {
            return nullptr;
        }
    } else { // realloc
        if (nsize <= osize) {
            if (!allocator) {
                return ptr;
            }

            try {
                std::size_t nsize_as_units = bytes_to_units(nsize);
                interprocess::offset_ptr<unit_type> as_offset_ptr{
                    static_cast<unit_type*>(ptr)};
                bool success{allocator->allocation_command(
                    interprocess::try_shrink_in_place,
                    bytes_to_units(osize), nsize_as_units, as_offset_ptr)};
                if (!success) {
                    throw interprocess::bad_alloc{};
                }
                return as_offset_ptr.get();
            } catch (const interprocess::bad_alloc&) {
                // According to Programming in Lua 3rd edition § 32.1 ¶ 8, Lua
                // is unable to recover from allocation shrinking failures.
                // Therefore we just return a success value, but put the
                // allocator into an invalid state where new allocations will
                // always fail.
                allocator.reset();
                return ptr;
            }
        } else {
            assert(ptr); //< malloc is handled separately
            if (!allocator) {
                return nullptr;
            }

            auto ret = allocator->allocate(bytes_to_units(nsize)).get();
            if (!ret) {
                return nullptr;
            }

            std::memcpy(ret, ptr, osize);
            allocator->deallocate(
                static_cast<unit_type*>(ptr), bytes_to_units(osize));
            return ret;
        }
    }
}

} // namespace emilua

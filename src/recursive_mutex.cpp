/* Copyright (c) 2020, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <limits>
#include <deque>

#include <fmt/format.h>

#include <emilua/recursive_mutex.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char recursive_mutex_key;

EMILUA_GPERF_DECLS_BEGIN(recursive_mutex)
EMILUA_GPERF_NAMESPACE(emilua)
struct recursive_mutex_handle
{
    std::deque<lua_State*> pending;
    std::size_t nlocked = 0;
    lua_State* owner = nullptr;
};

static char recursive_mutex_mt_key;
EMILUA_GPERF_DECLS_END(recursive_mutex)

EMILUA_GPERF_DECLS_BEGIN(recursive_mutex)
EMILUA_GPERF_NAMESPACE(emilua)
static int recursive_mutex_lock(lua_State* L)
{
    auto handle = static_cast<recursive_mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto& vm_ctx = get_vm_context(L);
    auto current_fiber = vm_ctx.current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED_ASSUMING_INTERRUPTION_DISABLED(vm_ctx, L);

    if (handle->owner) {
        if (handle->owner == current_fiber) {
            if (
                handle->nlocked ==
                std::numeric_limits<decltype(handle->nlocked)>::max()
            ) {
                push(L, std::errc::value_too_large);
                return lua_error(L);
            }

            ++handle->nlocked;
            return 0;
        } else {
            handle->pending.emplace_back(current_fiber);
            return lua_yield(L, 0);
        }
    }

    handle->owner = current_fiber;
    assert(handle->nlocked == 0);
    handle->nlocked = 1;
    return 0;
}

static int recursive_mutex_try_lock(lua_State* L)
{
    auto handle = static_cast<recursive_mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto current_fiber = get_vm_context(L).current_fiber();

    if (handle->owner) {
        if (handle->owner == current_fiber) {
            if (
                handle->nlocked ==
                std::numeric_limits<decltype(handle->nlocked)>::max()
            ) {
                push(L, std::errc::value_too_large);
                return lua_error(L);
            }

            ++handle->nlocked;
            lua_pushboolean(L, 1);
            return 1;
        } else {
            lua_pushboolean(L, 0);
            return 1;
        }
    }

    handle->owner = current_fiber;
    assert(handle->nlocked == 0);
    handle->nlocked = 1;
    lua_pushboolean(L, 1);
    return 1;
}

static int recursive_mutex_unlock(lua_State* L)
{
    auto handle = static_cast<recursive_mutex_handle*>(lua_touserdata(L, 1));
    if (!handle || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_mutex_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto& vm_ctx = get_vm_context(L);

    if (handle->owner != vm_ctx.current_fiber()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    if (--handle->nlocked != 0)
        return 0;

    if (handle->pending.size() == 0) {
        handle->owner = nullptr;
        return 0;
    }

    auto next = handle->pending.front();
    handle->pending.pop_front();
    handle->owner = next;
    handle->nlocked = 1;
    vm_ctx.strand().post([vm_ctx=vm_ctx.shared_from_this(),next]() {
        vm_ctx->fiber_resume(
            next, hana::make_set(vm_context::options::skip_clear_interrupter));
    }, std::allocator<void>{});
    return 0;
}
EMILUA_GPERF_DECLS_END(recursive_mutex)

static int recursive_mutex_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "unlock",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, recursive_mutex_unlock);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lock",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, recursive_mutex_lock);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "try_lock",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, recursive_mutex_try_lock);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int recursive_mutex_new(lua_State* L)
{
    auto buf = static_cast<recursive_mutex_handle*>(
        lua_newuserdata(L, sizeof(recursive_mutex_handle))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_mutex_mt_key);
    setmetatable(L, -2);
    new (buf) recursive_mutex_handle{};
    return 1;
}

void init_recursive_mutex_module(lua_State* L)
{
    lua_pushlightuserdata(L, &recursive_mutex_key);
    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "recursive_mutex");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                auto key = tostringview(L, 2);
                if (key == "new") {
                    lua_pushcfunction(L, recursive_mutex_new);
                    return 1;
                } else {
                    push(L, errc::bad_index, "index", 2);
                    return lua_error(L);
                }
            });
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(
            L,
            [](lua_State* L) -> int {
                push(L, std::errc::operation_not_permitted);
                return lua_error(L);
            });
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &recursive_mutex_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "recursive_mutex");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, recursive_mutex_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<recursive_mutex_handle>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

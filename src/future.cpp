/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <boost/container/small_vector.hpp>
#include <boost/scope_exit.hpp>

#include <emilua/detail/core.hpp>
#include <emilua/async_base.hpp>
#include <emilua/future.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

char future_key;

EMILUA_GPERF_DECLS_BEGIN(future)
EMILUA_GPERF_NAMESPACE(emilua)
static char future_shared_state_mt_key;
static char promise_mt_key;
static char future_mt_key;
static char future_get_key;

struct future_shared_state
{
    boost::container::small_vector<lua_State*, 1> waiters;
    enum : char
    {
        empty,
        broken,
        value_ready,
        error_ready
    } state = empty;
    int value_ref = LUA_NOREF;
};
EMILUA_GPERF_DECLS_END(future)

static int future_shared_state_mt_gc(lua_State* L)
{
    auto& state = *static_cast<future_shared_state*>(lua_touserdata(L, 1));
    BOOST_SCOPE_EXIT_ALL(&) { state.~future_shared_state(); };

    switch (state.state) {
    case future_shared_state::empty:
    case future_shared_state::broken:
        return 0;
    case future_shared_state::value_ready:
    case future_shared_state::error_ready:
        break;
    }

    luaL_unref(L, LUA_REGISTRYINDEX, state.value_ref);
    return 0;
}

EMILUA_GPERF_DECLS_BEGIN(promise)
EMILUA_GPERF_NAMESPACE(emilua)
static int promise_set_value(lua_State* L)
{
    lua_settop(L, 2);

    if (!lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &promise_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_getfenv(L, 1);
    lua_rawgeti(L, -1, 1);
    auto& state = *static_cast<future_shared_state*>(lua_touserdata(L, -1));

    if (state.state != future_shared_state::empty) {
        push(L, errc::promise_already_satisfied);
        return lua_error(L);
    }

    lua_pushvalue(L, 2);
    state.value_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    state.state = future_shared_state::value_ready;

    auto vm_ctx = get_vm_context(L).shared_from_this();
    for (auto& fiber : state.waiters) {
        vm_ctx->strand().post([vm_ctx,fiber,ref=state.value_ref]() {
            auto push_value = [&ref](lua_State* L) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            };

            vm_ctx->fiber_resume(
                fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(std::nullopt, push_value))));
        }, std::allocator<void>{});
    }
    state.waiters.clear();
    return 0;
}

static int promise_set_error(lua_State* L)
{
    lua_settop(L, 2);

    if (!lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &promise_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (lua_isnil(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_getfenv(L, 1);
    lua_rawgeti(L, -1, 1);
    auto& state = *static_cast<future_shared_state*>(lua_touserdata(L, -1));

    if (state.state != future_shared_state::empty) {
        push(L, errc::promise_already_satisfied);
        return lua_error(L);
    }

    lua_pushvalue(L, 2);
    state.value_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    state.state = future_shared_state::error_ready;

    auto vm_ctx = get_vm_context(L).shared_from_this();
    for (auto& fiber : state.waiters) {
        vm_ctx->strand().post([vm_ctx,fiber,ref=state.value_ref]() {
            auto push_value = [&ref](lua_State* L) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
            };

            vm_ctx->fiber_resume(
                fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(push_value))));
        }, std::allocator<void>{});
    }
    state.waiters.clear();
    return 0;
}
EMILUA_GPERF_DECLS_END(promise)

static int promise_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "set_value",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, promise_set_value);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "set_error",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, promise_set_error);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int promise_mt_gc(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &detail::context_key);
    auto vm_ctx = static_cast<vm_context*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!vm_ctx) {
        // VM is closing and context_key in LUA_REGISTRYINDEX was collected
        // already
        return 0;
    }

    lua_getfenv(L, 1);
    if (lua_isnil(L, -1)) {
        // VM is closing and userdata's env was collected already
        return 0;
    }

    lua_rawgeti(L, -1, 1);
    auto state = static_cast<future_shared_state*>(lua_touserdata(L, -1));
    if (!state) {
        // VM is closing and userdata's env contents were collected already
        return 0;
    }

    state->state = future_shared_state::broken;

    std::shared_ptr<vm_context> vm_ctx2;
    try {
        vm_ctx2 = vm_ctx->shared_from_this();
    } catch (const std::bad_weak_ptr&) {
        // reached here from VM shutdown, so just ignore waiters
        return 0;
    }
    for (auto& fiber : state->waiters) {
        vm_ctx->strand().post([vm_ctx2,fiber]() {
            vm_ctx2->fiber_resume(
                fiber,
                hana::make_set(
                    hana::make_pair(
                        vm_context::options::arguments,
                        hana::make_tuple(errc::broken_promise))));
        }, std::allocator<void>{});
    }
    state->waiters.clear();
    return 0;
}

EMILUA_GPERF_DECLS_BEGIN(future)
EMILUA_GPERF_NAMESPACE(emilua)
static int future_get(lua_State* L)
{
    if (!lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &future_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto& vm_ctx = get_vm_context(L);
    auto current_fiber = vm_ctx.current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    lua_getfenv(L, 1);
    lua_rawgeti(L, -1, 1);
    auto& state = *static_cast<future_shared_state*>(lua_touserdata(L, -1));

    switch (state.state) {
    case future_shared_state::broken:
        push(L, errc::broken_promise);
        return lua_error(L);
    case future_shared_state::value_ready:
        lua_pushnil(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, state.value_ref);
        return 2;
    case future_shared_state::error_ready:
        lua_rawgeti(L, LUA_REGISTRYINDEX, state.value_ref);
        return lua_error(L);
    case future_shared_state::empty:
        break;
    default:
        assert(false);
    }

    lua_pushvalue(L, -1);
    lua_pushlightuserdata(L, current_fiber);
    lua_pushcclosure(
        L,
        [](lua_State* L) -> int {
            auto vm_ctx = get_vm_context(L).shared_from_this();
            auto& state = *static_cast<future_shared_state*>(
                lua_touserdata(L, lua_upvalueindex(1)));
            auto current_fiber = static_cast<lua_State*>(
                lua_touserdata(L, lua_upvalueindex(2)));

            auto it = std::find(state.waiters.begin(), state.waiters.end(),
                                current_fiber);
            if (it == state.waiters.end()) {
                // set_value() was called before interrupt()
                return 0;
            }

            state.waiters.erase(it);
            vm_ctx->strand().post([vm_ctx,current_fiber]() {
                auto opt_args = vm_context::options::arguments;
                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        hana::make_pair(
                            opt_args, hana::make_tuple(errc::interrupted))));
            }, std::allocator<void>{});
            return 0;
        },
        2);
    set_interrupter(L, vm_ctx);

    state.waiters.emplace_back(current_fiber);
    return lua_yield(L, 0);
}
EMILUA_GPERF_DECLS_END(future)

static int future_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "get",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &future_get_key);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int future_mt_gc(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &detail::context_key);
    auto vm_ctx = static_cast<vm_context*>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    if (!vm_ctx) {
        // VM is closing and context_key in LUA_REGISTRYINDEX was collected
        // already
        return 0;
    }

    lua_getfenv(L, 1);
    if (lua_isnil(L, -1)) {
        // VM is closing and userdata's env was collected already
        return 0;
    }

    lua_rawgeti(L, -1, 1);
    auto state = static_cast<future_shared_state*>(lua_touserdata(L, -1));
    if (!state) {
        // VM is closing and userdata's env contents were collected already
        return 0;
    }

    constexpr auto spec{FMT_STRING(
        "Underlying promise for future {} is broken"
    )};
    if (state->waiters.size() != 0)
        vm_ctx->notify_deadlock(fmt::format(spec, lua_touserdata(L, 1)));
    return 0;
}

static int future_new(lua_State* L)
{
    lua_settop(L, 0);

    auto state = static_cast<future_shared_state*>(
        lua_newuserdata(L, sizeof(future_shared_state))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &future_shared_state_mt_key);
    setmetatable(L, -2);
    new (state) future_shared_state{};

    lua_createtable(L, /*narr=*/1, /*nrec=*/0);
    lua_pushvalue(L, 1);
    lua_rawseti(L, -2, 1);

    lua_newuserdata(L, /*size=*/1);
    lua_pushvalue(L, 2);
    lua_setfenv(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &promise_mt_key);
    setmetatable(L, -2);

    lua_newuserdata(L, /*size=*/1);
    lua_pushvalue(L, 2);
    lua_setfenv(L, -2);
    rawgetp(L, LUA_REGISTRYINDEX, &future_mt_key);
    setmetatable(L, -2);

    return 2;
}

void init_future(lua_State* L)
{
    lua_pushlightuserdata(L, &future_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "new");
        lua_pushcfunction(L, future_new);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &future_shared_state_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, future_shared_state_mt_gc);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &promise_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "promise");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, promise_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, promise_mt_gc);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &future_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "future");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, future_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, future_mt_gc);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &future_get_key);
    rawgetp(L, LUA_REGISTRYINDEX,
            &var_args__retval1_to_error__fwd_retval2__key);
    rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
    lua_pushcfunction(L, future_get);
    lua_call(L, 2, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

/* Copyright (c) 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <boost/predef/library/std.h>

#include <emilua/filesystem.hpp>
#include <emilua/windows.hpp>
#include <emilua/system.hpp>
#include <emilua/actor.hpp>
#include <emilua/time.hpp>

#include <boost/scope_exit.hpp>

#if BOOST_OS_UNIX
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#endif // BOOST_OS_UNIX

#if BOOST_OS_LINUX
#include <sys/capability.h>
#include <sys/sysmacros.h>
#endif // BOOST_OS_LINUX
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

EMILUA_GPERF_DECLS_BEGIN(filesystem)
EMILUA_GPERF_NAMESPACE(emilua)
namespace fs = std::filesystem;

extern unsigned char mode_bytecode[];
extern std::size_t mode_bytecode_size;

char filesystem_key;
char filesystem_path_mt_key;
static char space_info_mt_key;
static char directory_iterator_mt_key;
static char path_ctors_key;
static char clock_ctors_key;
static char mode_key;

struct directory_iterator
{
    template<class... Args>
    directory_iterator(Args&&... args)
        : iterator{std::forward<Args>(args)...}
    {}

    fs::directory_iterator iterator;
    bool increment = false;

    static int next(lua_State* L);
    static int make(lua_State* L);
};

static char filesystem_path_iterator_mt_key;
static char file_clock_time_point_mt_key;
static char file_status_mt_key;
static char directory_entry_mt_key;
static char recursive_directory_iterator_mt_key;

using lua_Seconds = std::chrono::duration<lua_Number>;

struct recursive_directory_iterator
{
    template<class... Args>
    recursive_directory_iterator(Args&&... args)
        : iterator{std::forward<Args>(args)...}
    {}

    fs::recursive_directory_iterator iterator;
    bool increment = false;

    static int pop(lua_State* L);
    static int disable_recursion_pending(lua_State* L);
    static int recursion_pending(lua_State* L);
    static int mt_index(lua_State* L);
    static int next(lua_State* L);
    static int make(lua_State* L);
};
EMILUA_GPERF_DECLS_END(filesystem)

EMILUA_GPERF_DECLS_BEGIN(path)
EMILUA_GPERF_NAMESPACE(emilua)
static int path_to_generic(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        auto ret = path->generic_u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int path_iterator(lua_State* L)
{
    static constexpr auto iter = [](lua_State* L) {
        auto path = static_cast<fs::path*>(
            lua_touserdata(L, lua_upvalueindex(1)));
        auto iter = static_cast<fs::path::iterator*>(
            lua_touserdata(L, lua_upvalueindex(2)));

        if (*iter == path->end())
            return 0;

        try {
            auto ret = (*iter)->u8string();
            ++*iter;
            lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    };

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushvalue(L, 1);
    {
        auto iter = static_cast<fs::path::iterator*>(
            lua_newuserdata(L, sizeof(fs::path::iterator)));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_iterator_mt_key);
        setmetatable(L, -2);
        new (iter) fs::path::iterator{};
        try {
            *iter = path->begin();
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    lua_pushcclosure(L, iter, 2);
    return 1;
}

static int path_make_preferred(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->make_preferred();

    return 1;
}

static int path_remove_filename(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->remove_filename();

    return 1;
}

static int path_replace_filename(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->replace_filename(path2);

    return 1;
}

static int path_replace_extension(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{*path};
    ret->replace_extension(path2);

    return 1;
}

static int path_lexically_normal(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->lexically_normal();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_lexically_relative(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->lexically_relative(path2);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

static int path_lexically_proximate(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->lexically_proximate(path2);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

inline int path_root_name(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->root_name().u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_root_directory(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->root_directory().u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_root_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->root_path();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

inline int path_relative_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->relative_path();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

inline int path_parent_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    try {
        *ret = path->parent_path();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    return 1;
}

inline int path_filename(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->filename().u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_stem(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->stem().u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_extension(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->extension().u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_empty(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    lua_pushboolean(L, path->empty());
    return 1;
}

inline int path_has_root_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_root_path());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_root_name(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_root_name());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_root_directory(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_root_directory());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_relative_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_relative_path());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_parent_path(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_parent_path());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_filename(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_filename());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_stem(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_stem());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_has_extension(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->has_extension());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_is_absolute(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->is_absolute());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

inline int path_is_relative(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    try {
        lua_pushboolean(L, path->is_relative());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}
EMILUA_GPERF_DECLS_END(path)

static int path_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "to_generic",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_to_generic);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "iterator",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_iterator);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "make_preferred",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_make_preferred);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "remove_filename",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_remove_filename);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "replace_filename",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_replace_filename);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "replace_extension",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_replace_extension);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lexically_normal",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_lexically_normal);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lexically_relative",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_lexically_relative);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lexically_proximate",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_lexically_proximate);
                return 1;
            })
        EMILUA_GPERF_PAIR("root_name", path_root_name)
        EMILUA_GPERF_PAIR("root_directory", path_root_directory)
        EMILUA_GPERF_PAIR("root_path", path_root_path)
        EMILUA_GPERF_PAIR("relative_path", path_relative_path)
        EMILUA_GPERF_PAIR("parent_path", path_parent_path)
        EMILUA_GPERF_PAIR("filename", path_filename)
        EMILUA_GPERF_PAIR("stem", path_stem)
        EMILUA_GPERF_PAIR("extension", path_extension)
        EMILUA_GPERF_PAIR("empty", path_empty)
        EMILUA_GPERF_PAIR("has_root_path", path_has_root_path)
        EMILUA_GPERF_PAIR("has_root_name", path_has_root_name)
        EMILUA_GPERF_PAIR("has_root_directory", path_has_root_directory)
        EMILUA_GPERF_PAIR("has_relative_path", path_has_relative_path)
        EMILUA_GPERF_PAIR("has_parent_path", path_has_parent_path)
        EMILUA_GPERF_PAIR("has_filename", path_has_filename)
        EMILUA_GPERF_PAIR("has_stem", path_has_stem)
        EMILUA_GPERF_PAIR("has_extension", path_has_extension)
        EMILUA_GPERF_PAIR("is_absolute", path_is_absolute)
        EMILUA_GPERF_PAIR("is_relative", path_is_relative)
    EMILUA_GPERF_END(key)(L);
}

static int path_mt_tostring(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));

    try {
        auto ret = path->u8string();
        lua_pushlstring(L, reinterpret_cast<char*>(ret.data()), ret.size());
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int path_mt_eq(lua_State* L)
{
    auto path1 = static_cast<fs::path*>(lua_touserdata(L, 1));
    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *path1 == *path2);
    return 1;
}

static int path_mt_lt(lua_State* L)
{
    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, path1 < path2);
    return 1;
}

static int path_mt_le(lua_State* L)
{
    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, path1 <= path2);
    return 1;
}

static int path_mt_div(lua_State* L)
{
    lua_settop(L, 2);

    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};
    *path = path1 / path2;

    return 1;
}

static int path_mt_concat(lua_State* L)
{
    lua_settop(L, 2);

    fs::path path1, path2;

    switch (lua_type(L, 1)) {
    case LUA_TSTRING:
        try {
            path1 = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
        if (!path || !lua_getmetatable(L, 1)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 1);
            return lua_error(L);
        }

        path1 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TSTRING:
        try {
            path2 = fs::path{
                widen_on_windows(tostringview(L, 2)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    case LUA_TUSERDATA: {
        auto path = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *path;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{path1};
    *path += path2;

    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(path_ctors)
EMILUA_GPERF_NAMESPACE(emilua)
static int path_new(lua_State* L)
{
    lua_settop(L, 1);

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};

    switch (lua_type(L, 1)) {
    case LUA_TNIL:
        break;
    case LUA_TSTRING:
        try {
            *path = fs::path{
                widen_on_windows(tostringview(L, 1)), fs::path::native_format};
            break;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    default:
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    return 1;
}

static int path_from_generic(lua_State* L)
{
    if (lua_type(L, 1) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};

    try {
        *path = fs::path{
            widen_on_windows(tostringview(L, 1)), fs::path::generic_format};
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}
EMILUA_GPERF_DECLS_END(path_ctors)

EMILUA_GPERF_DECLS_BEGIN(clock)
EMILUA_GPERF_NAMESPACE(emilua)
static int file_clock_time_point_add(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::file_clock::duration::max() ||
        dur < std::chrono::file_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp += std::chrono::round<std::chrono::file_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

static int file_clock_time_point_sub(lua_State* L)
{
    lua_settop(L, 2);

    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::file_clock::duration::max() ||
        dur < std::chrono::file_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    try {
        *tp -= std::chrono::round<std::chrono::file_clock::duration>(dur);
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

static int file_clock_time_point_to_system(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::system_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::system_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::system_clock::time_point{};
#if BOOST_LIB_STD_GNU || BOOST_LIB_STD_CXX
    // current libstdc++ hasn't got clock_cast yet
    *ret = std::chrono::file_clock::to_sys(*tp);
#else
    *ret = std::chrono::clock_cast<std::chrono::system_clock>(*tp);
#endif
    return 1;
}

inline int file_clock_time_point_seconds_since_epoch(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    lua_pushnumber(L, lua_Seconds{tp->time_since_epoch()}.count());
    return 1;
}
EMILUA_GPERF_DECLS_END(clock)

static int file_clock_time_point_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "add",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_clock_time_point_add);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "sub",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_clock_time_point_sub);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "to_system",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_clock_time_point_to_system);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "seconds_since_epoch", file_clock_time_point_seconds_since_epoch)
    EMILUA_GPERF_END(key)(L);
}

static int file_clock_time_point_mt_eq(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 2));
    lua_pushboolean(L, *tp1 == *tp2);
    return 1;
}

static int file_clock_time_point_mt_lt(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 < *tp2);
    return 1;
}

static int file_clock_time_point_mt_le(lua_State* L)
{
    auto tp1 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp1 || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 2));
    if (!tp2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    lua_pushboolean(L, *tp1 <= *tp2);
    return 1;
}

static int file_clock_time_point_mt_add(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_Number secs = luaL_checknumber(L, 2);
    if (std::isnan(secs) || std::isinf(secs)) {
        push(L, std::errc::argument_out_of_domain, "arg", 2);
        return lua_error(L);
    }

    lua_Seconds dur{secs};
    if (
        dur > std::chrono::file_clock::duration::max() ||
        dur < std::chrono::file_clock::duration::min()
    ) {
        push(L, std::errc::value_too_large);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::file_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::file_clock::time_point{};

    try {
        *ret = *tp +
            std::chrono::round<std::chrono::file_clock::duration>(dur);
        return 1;
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
}

static int file_clock_time_point_mt_sub(lua_State* L)
{
    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TNUMBER: {
        lua_Number secs = lua_tonumber(L, 2);
        if (std::isnan(secs) || std::isinf(secs)) {
            push(L, std::errc::argument_out_of_domain, "arg", 2);
            return lua_error(L);
        }

        lua_Seconds dur{secs};
        if (
            dur > std::chrono::file_clock::duration::max() ||
            dur < std::chrono::file_clock::duration::min()
        ) {
            push(L, std::errc::value_too_large);
            return lua_error(L);
        }

        auto ret = static_cast<std::chrono::file_clock::time_point*>(
            lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        setmetatable(L, -2);
        new (ret) std::chrono::file_clock::time_point{};

        try {
            *ret = *tp -
                std::chrono::round<std::chrono::file_clock::duration>(dur);
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    case LUA_TUSERDATA: {
        auto tp2 = static_cast<std::chrono::file_clock::time_point*>(
            lua_touserdata(L, 2));
        if (!tp2 || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        try {
            lua_pushnumber(L, lua_Seconds{*tp - *tp2}.count());
            return 1;
        } catch (const std::system_error& e) {
            push(L, e.code());
            return lua_error(L);
        } catch (const std::exception& e) {
            lua_pushstring(L, e.what());
            return lua_error(L);
        }
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

EMILUA_GPERF_DECLS_BEGIN(space_info)
EMILUA_GPERF_NAMESPACE(emilua)
inline int space_info_capacity(lua_State* L)
{
    auto space = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    lua_pushinteger(L, space->capacity);
    return 1;
}

inline int space_info_free(lua_State* L)
{
    auto space = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    lua_pushinteger(L, space->free);
    return 1;
}

inline int space_info_available(lua_State* L)
{
    auto space = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    lua_pushinteger(L, space->available);
    return 1;
}
EMILUA_GPERF_DECLS_END(space_info)

static int space_info_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR("capacity", space_info_capacity)
        EMILUA_GPERF_PAIR("free", space_info_free)
        EMILUA_GPERF_PAIR("available", space_info_available)
    EMILUA_GPERF_END(key)(L);
}

static int space_info_mt_eq(lua_State* L)
{
    auto sp1 = static_cast<fs::space_info*>(lua_touserdata(L, 1));
    auto sp2 = static_cast<fs::space_info*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *sp1 == *sp2);
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(file_status)
EMILUA_GPERF_NAMESPACE(emilua)
inline int file_status_type(lua_State* L)
{
    auto st = static_cast<fs::file_status*>(lua_touserdata(L, 1));
    switch (st->type()) {
    case fs::file_type::none:
        lua_pushnil(L);
        return 1;
    case fs::file_type::not_found:
        lua_pushliteral(L, "not_found");
        return 1;
    case fs::file_type::regular:
        lua_pushliteral(L, "regular");
        return 1;
    case fs::file_type::directory:
        lua_pushliteral(L, "directory");
        return 1;
    case fs::file_type::symlink:
        lua_pushliteral(L, "symlink");
        return 1;
    case fs::file_type::block:
        lua_pushliteral(L, "block");
        return 1;
    case fs::file_type::character:
        lua_pushliteral(L, "character");
        return 1;
    case fs::file_type::fifo:
        lua_pushliteral(L, "fifo");
        return 1;
    case fs::file_type::socket:
        lua_pushliteral(L, "socket");
        return 1;
#if BOOST_OS_WINDOWS
    case fs::file_type::junction:
        lua_pushliteral(L, "junction");
        return 1;
#endif // BOOST_OS_WINDOWS
    case fs::file_type::unknown:
        // by avoiding an explicit `default` case we get compiler warnings in
        // case we miss any
        break;
    }

    lua_pushliteral(L, "unknown");
    return 1;
}

inline int file_status_mode(lua_State* L)
{
    auto st = static_cast<fs::file_status*>(lua_touserdata(L, 1));
    if (st->permissions() == fs::perms::unknown) {
        lua_pushliteral(L, "unknown");
        return 1;
    }
    auto p = static_cast<std::underlying_type_t<fs::perms>>(st->permissions());
    lua_pushinteger(L, p);
    return 1;
}
EMILUA_GPERF_DECLS_END(file_status)

static int file_status_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR("type", file_status_type)
        EMILUA_GPERF_PAIR("mode", file_status_mode)
    EMILUA_GPERF_END(key)(L);
}

static int file_status_mt_eq(lua_State* L)
{
    auto st1 = static_cast<fs::file_status*>(lua_touserdata(L, 1));
    auto st2 = static_cast<fs::file_status*>(lua_touserdata(L, 2));
    lua_pushboolean(L, *st1 == *st2);
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(directory_entry)
EMILUA_GPERF_NAMESPACE(emilua)
static int directory_entry_refresh(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));
    if (!entry || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &directory_entry_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    entry->refresh(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

inline int directory_entry_path(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));
    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};
    *path = entry->path();
    return 1;
}

inline int directory_entry_file_size(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->file_size(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

inline int directory_entry_hard_link_count(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->hard_link_count(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

inline int directory_entry_last_write_time(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->last_write_time(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto tp = static_cast<std::chrono::file_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (tp) std::chrono::file_clock::time_point{ret};
    return 1;
}

inline int directory_entry_status(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->status(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto st = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (st) fs::file_status{ret};
    return 1;
}

inline int directory_entry_symlink_status(lua_State* L)
{
    auto entry = static_cast<fs::directory_entry*>(lua_touserdata(L, 1));

    std::error_code ec;
    auto ret = entry->symlink_status(ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = entry->path();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto st = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (st) fs::file_status{ret};
    return 1;
}
EMILUA_GPERF_DECLS_END(directory_entry)

static int directory_entry_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "refresh",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, directory_entry_refresh);
                return 1;
            })
        EMILUA_GPERF_PAIR("path", directory_entry_path)
        EMILUA_GPERF_PAIR("file_size", directory_entry_file_size)
        EMILUA_GPERF_PAIR("hard_link_count", directory_entry_hard_link_count)
        EMILUA_GPERF_PAIR("last_write_time", directory_entry_last_write_time)
        EMILUA_GPERF_PAIR("status", directory_entry_status)
        EMILUA_GPERF_PAIR("symlink_status", directory_entry_symlink_status)
    EMILUA_GPERF_END(key)(L);
}

int directory_iterator::next(lua_State* L)
{
    auto self = static_cast<directory_iterator*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    if (self->iterator == fs::directory_iterator{})
        return 0;

    if (self->increment) {
        std::error_code ec;
        self->iterator.increment(ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        if (self->iterator == fs::directory_iterator{})
            return 0;
    } else {
        self->increment = true;
    }

    auto ret = static_cast<fs::directory_entry*>(
        lua_newuserdata(L, sizeof(fs::directory_entry)));
    rawgetp(L, LUA_REGISTRYINDEX, &directory_entry_mt_key);
    setmetatable(L, -2);
    new (ret) fs::directory_entry{};
    *ret = *(self->iterator);
    return 1;
}

int directory_iterator::make(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::directory_options options = fs::directory_options::none;

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, 2, "skip_permission_denied");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::directory_options::skip_permission_denied;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "skip_permission_denied");
            return lua_error(L);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    {
        std::error_code ec;
        auto iter = static_cast<directory_iterator*>(
            lua_newuserdata(L, sizeof(directory_iterator)));
        rawgetp(L, LUA_REGISTRYINDEX, &directory_iterator_mt_key);
        setmetatable(L, -2);
        new (iter) directory_iterator{*path, options, ec};

        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
    }
    lua_pushcclosure(L, next, 1);
    return 1;
}

EMILUA_GPERF_DECLS_BEGIN(recursive_directory_iterator)
EMILUA_GPERF_NAMESPACE(emilua)
int recursive_directory_iterator::pop(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, 1));
    if (!self || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_directory_iterator_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    self->iterator.pop(ec);
    if (ec) {
        push(L, ec);
        return lua_error(L);
    }
    return 0;
}

int recursive_directory_iterator::disable_recursion_pending(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, 1));
    if (!self || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &recursive_directory_iterator_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (self->iterator == fs::recursive_directory_iterator{}) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    try {
        self->iterator.disable_recursion_pending();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }
    return 0;
}

int recursive_directory_iterator::recursion_pending(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, 1));
    if (self->iterator == fs::recursive_directory_iterator{}) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    lua_pushboolean(L, self->iterator.recursion_pending());
    return 1;
}
EMILUA_GPERF_DECLS_END(recursive_directory_iterator)

int recursive_directory_iterator::mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "pop",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, recursive_directory_iterator::pop);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "disable_recursion_pending",
            [](lua_State* L) -> int {
                lua_pushcfunction(
                    L, recursive_directory_iterator::disable_recursion_pending);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "recursion_pending",
            recursive_directory_iterator::recursion_pending)
    EMILUA_GPERF_END(key)(L);
}

int recursive_directory_iterator::next(lua_State* L)
{
    auto self = static_cast<recursive_directory_iterator*>(
        lua_touserdata(L, lua_upvalueindex(1)));

    if (self->iterator == fs::recursive_directory_iterator{})
        return 0;

    if (self->increment) {
        std::error_code ec;
        self->iterator.increment(ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }

        if (self->iterator == fs::recursive_directory_iterator{})
            return 0;
    } else {
        self->increment = true;
    }

    auto ret = static_cast<fs::directory_entry*>(
        lua_newuserdata(L, sizeof(fs::directory_entry)));
    rawgetp(L, LUA_REGISTRYINDEX, &directory_entry_mt_key);
    setmetatable(L, -2);
    new (ret) fs::directory_entry{};
    *ret = *(self->iterator);

    lua_pushinteger(L, self->iterator.depth());

    return 2;
}

int recursive_directory_iterator::make(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::directory_options options = fs::directory_options::none;

    switch (lua_type(L, 2)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, 2, "skip_permission_denied");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::directory_options::skip_permission_denied;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "skip_permission_denied");
            return lua_error(L);
        }

        lua_getfield(L, 2, "follow_directory_symlink");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::directory_options::follow_directory_symlink;
            break;
        default:
            push(L, std::errc::invalid_argument,
                 "arg", "follow_directory_symlink");
            return lua_error(L);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    {
        std::error_code ec;
        auto iter = static_cast<recursive_directory_iterator*>(
            lua_newuserdata(L, sizeof(recursive_directory_iterator)));
        rawgetp(L, LUA_REGISTRYINDEX, &recursive_directory_iterator_mt_key);
        setmetatable(L, -2);
        new (iter) recursive_directory_iterator{*path, options, ec};

        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
    }
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, next, 1);
    lua_insert(L, -2);
    return 2;
}

EMILUA_GPERF_DECLS_BEGIN(clock_ctors)
EMILUA_GPERF_NAMESPACE(emilua)
static int file_clock_from_system(lua_State* L)
{
    auto tp = static_cast<std::chrono::system_clock::time_point*>(
        lua_touserdata(L, 1));
    if (!tp || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &system_clock_time_point_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<std::chrono::file_clock::time_point*>(
        lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
    setmetatable(L, -2);
    new (ret) std::chrono::file_clock::time_point{};
#if BOOST_LIB_STD_GNU || BOOST_LIB_STD_CXX
    // current libstdc++ hasn't got clock_cast yet
    *ret = std::chrono::file_clock::from_sys(*tp);
#else
    *ret = std::chrono::clock_cast<std::chrono::file_clock>(*tp);
#endif
    return 1;
}
EMILUA_GPERF_DECLS_END(clock_ctors)

EMILUA_GPERF_DECLS_BEGIN(filesystem)
EMILUA_GPERF_NAMESPACE(emilua)
static int absolute(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    std::error_code ec;
    *ret = fs::absolute(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int canonical(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    std::error_code ec;
    *ret = fs::canonical(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int weakly_canonical(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    std::error_code ec;
    *ret = fs::weakly_canonical(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int relative(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;
    switch (lua_type(L, 2)) {
    case LUA_TNIL: {
        std::error_code ec;
        path2 = fs::current_path(ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        break;
    }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    std::error_code ec;
    *ret = fs::relative(*path, path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        *ret = path2;
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 1;
}

static int proximate(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    fs::path path2;
    switch (lua_type(L, 2)) {
    case LUA_TNIL: {
        std::error_code ec;
        path2 = fs::current_path(ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        break;
    }
    case LUA_TUSERDATA: {
        auto p = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!p || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        path2 = *p;
        break;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    std::error_code ec;
    *ret = fs::proximate(*path, path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        *ret = path2;
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 1;
}

static int last_write_time(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TNIL: {
        auto tp = static_cast<std::chrono::file_clock::time_point*>(
            lua_newuserdata(L, sizeof(std::chrono::file_clock::time_point))
        );
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        setmetatable(L, -2);
        new (tp) std::chrono::file_clock::time_point{};
        std::error_code ec;
        *tp = fs::last_write_time(*path, ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        return 1;
    }
    case LUA_TUSERDATA: {
        auto tp = static_cast<std::chrono::file_clock::time_point*>(
            lua_touserdata(L, 2));
        if (!tp || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &file_clock_time_point_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        std::error_code ec;
        fs::last_write_time(*path, *tp, ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        return 0;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

static int copy(lua_State* L)
{
    lua_settop(L, 3);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    fs::copy_options options = fs::copy_options::none;
    switch (lua_type(L, 3)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, 3, "existing");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TSTRING: {
            auto v = tostringview(L);
            if (v == "skip") {
                options |= fs::copy_options::skip_existing;
            } else if (v == "overwrite") {
                options |= fs::copy_options::overwrite_existing;
            } else if (v == "update") {
                options |= fs::copy_options::update_existing;
            } else {
                push(L, std::errc::invalid_argument, "arg", "existing");
                return lua_error(L);
            }
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", "existing");
            return lua_error(L);
        }

        lua_getfield(L, 3, "recursive");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TBOOLEAN:
            if (lua_toboolean(L, -1))
                options |= fs::copy_options::recursive;
            break;
        default:
            push(L, std::errc::invalid_argument, "arg", "recursive");
            return lua_error(L);
        }

        lua_getfield(L, 3, "symlinks");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TSTRING: {
            auto v = tostringview(L);
            if (v == "copy") {
                options |= fs::copy_options::copy_symlinks;
            } else if (v == "skip") {
                options |= fs::copy_options::skip_symlinks;
            } else {
                push(L, std::errc::invalid_argument, "arg", "symlinks");
                return lua_error(L);
            }
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", "symlinks");
            return lua_error(L);
        }

        lua_getfield(L, 3, "copy");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TSTRING: {
            auto v = tostringview(L);
            if (v == "directories_only") {
                options |= fs::copy_options::directories_only;
            } else if (v == "create_symlinks") {
                options |= fs::copy_options::create_symlinks;
            } else if (v == "create_hard_links") {
                options |= fs::copy_options::create_hard_links;
            } else {
                push(L, std::errc::invalid_argument, "arg", "copy");
                return lua_error(L);
            }
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", "copy");
            return lua_error(L);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    try {
        fs::copy(*path, *path2, options);
        return 0;
    } catch (const fs::filesystem_error& e) {
        push(L, e.code());

        lua_pushliteral(L, "path1");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = e.path1();
        }
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        {
            auto path = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (path) fs::path{};
            *path = e.path2();
        }
        lua_rawset(L, -3);

        return lua_error(L);
    }
}

static int copy_file(lua_State* L)
{
    lua_settop(L, 3);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    fs::copy_options options = fs::copy_options::none;
    switch (lua_type(L, 3)) {
    case LUA_TNIL:
        break;
    case LUA_TTABLE:
        lua_getfield(L, 3, "existing");
        switch (lua_type(L, -1)) {
        case LUA_TNIL:
            break;
        case LUA_TSTRING: {
            auto v = tostringview(L);
            if (v == "skip") {
                options |= fs::copy_options::skip_existing;
            } else if (v == "overwrite") {
                options |= fs::copy_options::overwrite_existing;
            } else if (v == "update") {
                options |= fs::copy_options::update_existing;
            } else {
                push(L, std::errc::invalid_argument, "arg", "existing");
                return lua_error(L);
            }
            break;
        }
        default:
            push(L, std::errc::invalid_argument, "arg", "existing");
            return lua_error(L);
        }
        break;
    default:
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::copy_file(*path, *path2, options, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int copy_symlink(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::error_code ec;
    fs::copy_symlink(*path, *path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

static int create_directory(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    switch (lua_type(L, 2)) {
    case LUA_TNIL: {
        std::error_code ec;
        bool ret = fs::create_directory(*path, ec);
        if (ec) {
            push(L, ec);
            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);
            return lua_error(L);
        }
        lua_pushboolean(L, ret);
        return 1;
    }
    case LUA_TUSERDATA: {
        auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
        if (!path2 || !lua_getmetatable(L, 2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            push(L, std::errc::invalid_argument, "arg", 2);
            return lua_error(L);
        }

        std::error_code ec;
        bool ret = fs::create_directory(*path, *path2, ec);
        if (ec) {
            push(L, ec);

            lua_pushliteral(L, "path1");
            lua_pushvalue(L, 1);
            lua_rawset(L, -3);

            lua_pushliteral(L, "path2");
            lua_pushvalue(L, 2);
            lua_rawset(L, -3);

            return lua_error(L);
        }
        lua_pushboolean(L, ret);
        return 1;
    }
    default:
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
}

static int create_directories(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::create_directories(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int create_hard_link(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::error_code ec;
    fs::create_hard_link(*path, *path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

static int create_symlink(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::error_code ec;
    fs::create_symlink(*path, *path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

static int create_directory_symlink(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::error_code ec;
    fs::create_directory_symlink(*path, *path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

#if BOOST_OS_UNIX
static int mkfifo(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    mode_t mode = luaL_checkinteger(L, 2);

    if (::mkfifo(path->c_str(), mode) == -1) {
        push(L, std::error_code{errno, std::system_category()});

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        return lua_error(L);
    }

    return 0;
}

static int mknod(lua_State* L)
{
    lua_settop(L, 3);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    mode_t mode = luaL_checkinteger(L, 2);
    dev_t dev = luaL_checkinteger(L, 3);

    if (::mknod(path->c_str(), mode, dev) == -1) {
        push(L, std::error_code{errno, std::system_category()});

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        return lua_error(L);
    }

    return 0;
}

static int fs_makedev(lua_State* L)
{
    int major = luaL_checkinteger(L, 1);
    int minor = luaL_checkinteger(L, 2);

    lua_pushinteger(L, makedev(major, minor));
    return 1;
}
#endif // BOOST_OS_UNIX

static int equivalent(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::equivalent(*path, *path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int file_size(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    auto ret = fs::file_size(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int hard_link_count(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    auto ret = fs::hard_link_count(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

#if BOOST_OS_UNIX
static int chown(lua_State* L)
{
    lua_settop(L, 3);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    int res = ::chown(
        path->string().data(), luaL_checkinteger(L, 2),
        luaL_checkinteger(L, 3));
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int lchown(lua_State* L)
{
    lua_settop(L, 3);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    int res = ::lchown(
        path->string().data(), luaL_checkinteger(L, 2),
        luaL_checkinteger(L, 3));
    if (res == -1) {
        push(L, std::error_code{errno, std::system_category()});
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_UNIX

static int chmod(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    fs::permissions(*path, static_cast<fs::perms>(luaL_checkinteger(L, 2)), ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int lchmod(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    fs::permissions(
        *path, static_cast<fs::perms>(luaL_checkinteger(L, 2)),
        fs::perm_options::replace | fs::perm_options::nofollow, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int read_symlink(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (ret) fs::path{};

    std::error_code ec;
    *ret = fs::read_symlink(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int remove(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::remove(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int remove_all(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    auto ret = fs::remove_all(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushinteger(L, ret);
    return 1;
}

static int rename(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto path2 = static_cast<fs::path*>(lua_touserdata(L, 2));
    if (!path2 || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    std::error_code ec;
    fs::rename(*path, *path2, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        lua_pushliteral(L, "path2");
        lua_pushvalue(L, 2);
        lua_rawset(L, -3);

        return lua_error(L);
    }
    return 0;
}

static int resize_file(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    fs::resize_file(*path, luaL_checkinteger(L, 2), ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 0;
}

static int is_empty(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_empty(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    lua_pushboolean(L, ret);
    return 1;
}

static int current_working_directory(lua_State* L)
{
    lua_settop(L, 1);

    if (lua_isnil(L, 1)) {
        auto path = static_cast<fs::path*>(
            lua_newuserdata(L, sizeof(fs::path)));
        rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
        setmetatable(L, -2);
        new (path) fs::path{};

        std::error_code ec;
        *path = fs::current_path(ec);
        if (ec) {
            push(L, ec);
            return lua_error(L);
        }
        return 1;
    }

    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        // we intentionally leave "path1" out as the path has nothing to do with
        // the failure here
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

#if BOOST_OS_UNIX
    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };

    int mfd = -1;
    BOOST_SCOPE_EXIT_ALL(&) { if (mfd != -1) close(mfd); };

    std::string::size_type mfd_size;

    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        auto as_str = path->string();
        mfd_size = as_str.size() + 1; //< include nul terminator

        mfd = memfd_create("emilua/current_working_directory", /*flags=*/0);
        if (mfd == -1) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        if (ftruncate(mfd, mfd_size) == -1) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }

        write(mfd, as_str.data(), mfd_size);
    }
#endif // BOOST_OS_UNIX

    std::error_code ec;
    fs::current_path(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

#if BOOST_OS_UNIX
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::CHDIR;
        request.chdir_mfd_size = mfd_size;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int) * 2)];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * 2);

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * 2);

        {
            char* begin = (char*)CMSG_DATA(cmsg);
            char* it = begin;

            std::memcpy(it, &channel[1], sizeof(int));
            it += sizeof(int);

            std::memcpy(it, &mfd, sizeof(int));
        }

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }
#endif // BOOST_OS_UNIX

    return 0;
}

static int exists(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::exists(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_block_file(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_block_file(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_character_file(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_character_file(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_directory(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_directory(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_fifo(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_fifo(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_other(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_other(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_regular_file(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_regular_file(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_socket(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_socket(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int is_symlink(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    bool ret = fs::is_symlink(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }

    lua_pushboolean(L, ret ? 1 : 0);
    return 1;
}

static int space(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::error_code ec;
    auto ret = fs::space(*path, ec);
    if (ec) {
        push(L, ec);

        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        return lua_error(L);
    }

    auto space = static_cast<fs::space_info*>(
        lua_newuserdata(L, sizeof(fs::space_info))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &space_info_mt_key);
    setmetatable(L, -2);
    new (space) fs::space_info{ret};
    return 1;
}

static int status(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (ret) fs::file_status{};

    std::error_code ec;
    *ret = fs::status(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int symlink_status(lua_State* L)
{
    auto path = static_cast<fs::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto ret = static_cast<fs::file_status*>(
        lua_newuserdata(L, sizeof(fs::file_status))
    );
    rawgetp(L, LUA_REGISTRYINDEX, &file_status_mt_key);
    setmetatable(L, -2);
    new (ret) fs::file_status{};

    std::error_code ec;
    *ret = fs::symlink_status(*path, ec);
    if (ec) {
        push(L, ec);
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);
        return lua_error(L);
    }
    return 1;
}

static int temp_directory_path(lua_State* L)
{
    lua_settop(L, 0);

    auto path = static_cast<fs::path*>(lua_newuserdata(L, sizeof(fs::path)));
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    setmetatable(L, -2);
    new (path) fs::path{};

    try {
        *path = fs::temp_directory_path();
        return 1;
    } catch (const fs::filesystem_error& e) {
        push(L, e.code());

        *path = e.path1();
        lua_pushliteral(L, "path1");
        lua_pushvalue(L, 1);
        lua_rawset(L, -3);

        return lua_error(L);
    }
}

// apparently there *IS* a Windows version of umask(), but I'm not quite sure
// what it does, so I'm disabling this for now
#if BOOST_OS_UNIX
static int filesystem_umask(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);
    if (!vm_ctx.is_master()) {
        push(L, std::errc::operation_not_permitted);
        return lua_error(L);
    }

    mode_t mask = luaL_checkinteger(L, 1);

#if BOOST_OS_UNIX
    int channel[2] = { -1, -1 };
    BOOST_SCOPE_EXIT_ALL(&) {
        if (channel[0] != -1) close(channel[0]);
        if (channel[1] != -1) close(channel[1]);
    };
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        int res = pipe(channel);
        if (res != 0) {
            push(L, std::error_code{errno, std::system_category()});
            return lua_error(L);
        }
    }
#endif // BOOST_OS_UNIX

    mode_t res = umask(mask);
    lua_pushinteger(L, res);

#if BOOST_OS_UNIX
    if (vm_ctx.appctx.ipc_actor_service_sockfd != -1) {
        ipc_actor_start_vm_request request;
        std::memset(&request, 0, sizeof(request));
        request.type = ipc_actor_start_vm_request::UMASK;
        request.umask_mask = mask;

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        union {
            struct cmsghdr align;
            char buf[CMSG_SPACE(sizeof(int))];
        } cmsgu;
        msg.msg_control = cmsgu.buf;
        msg.msg_controllen = CMSG_SPACE(sizeof(int));

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        std::memcpy(CMSG_DATA(cmsg), &channel[1], sizeof(int));

        sendmsg(vm_ctx.appctx.ipc_actor_service_sockfd, &msg, MSG_NOSIGNAL);
        close(channel[1]);
        channel[1] = -1;

        char buf[1];
        auto nread = read(channel[0], &buf, 1);
        if (nread == -1 || nread == 0) {
            // as described in <https://ewontfix.com/17/> the only safe answer
            // is to SIGKILL when we cannot guarantee atomicity of failure
            std::exit(1);
        }
    }
#endif // BOOST_OS_UNIX

    return 1;
}
#endif // BOOST_OS_UNIX

#if BOOST_OS_LINUX
static int filesystem_cap_get_file(lua_State* L)
{
    auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string path2;
    try {
        path2 = path->string();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    cap_t caps = cap_get_file(path2.c_str());
    if (caps == NULL) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    BOOST_SCOPE_EXIT_ALL(&) {
        if (caps != NULL)
            cap_free(caps);
    };

    auto& caps2 = *static_cast<cap_t*>(lua_newuserdata(L, sizeof(cap_t)));
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    setmetatable(L, -2);
    caps2 = caps;
    caps = NULL;

    return 1;
}

static int filesystem_cap_set_file(lua_State* L)
{
    lua_settop(L, 2);

    auto path = static_cast<std::filesystem::path*>(lua_touserdata(L, 1));
    if (!path || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    std::string path2;
    try {
        path2 = path->string();
    } catch (const std::system_error& e) {
        push(L, e.code());
        return lua_error(L);
    } catch (const std::exception& e) {
        lua_pushstring(L, e.what());
        return lua_error(L);
    }

    auto caps = static_cast<cap_t*>(lua_touserdata(L, 2));
    if (!caps || !lua_getmetatable(L, 2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &linux_capabilities_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (cap_set_file(path2.c_str(), *caps) == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }
    return 0;
}
#endif // BOOST_OS_LINUX
EMILUA_GPERF_DECLS_END(filesystem)

static int path_ctors_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "new",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_new);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "from_generic",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, path_from_generic);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "preferred_separator",
            [](lua_State* L) -> int {
                fs::path::value_type sep = fs::path::preferred_separator;
                push(L, narrow_on_windows(&sep, 1));
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int clock_ctors_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "from_system",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_clock_from_system);
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

static int filesystem_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "path",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &path_ctors_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mode",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &mode_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "clock",
            [](lua_State* L) -> int {
                rawgetp(L, LUA_REGISTRYINDEX, &clock_ctors_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "directory_iterator",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, directory_iterator::make);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "recursive_directory_iterator",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, recursive_directory_iterator::make);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "absolute",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, absolute);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "canonical",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, canonical);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "weakly_canonical",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, weakly_canonical);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "relative",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, relative);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "proximate",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, proximate);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "last_write_time",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, last_write_time);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "copy",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, copy);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "copy_file",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, copy_file);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "copy_symlink",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, copy_symlink);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "create_directory",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, create_directory);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "create_directories",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, create_directories);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "create_hard_link",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, create_hard_link);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "create_symlink",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, create_symlink);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "create_directory_symlink",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, create_directory_symlink);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mkfifo",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, mkfifo);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "mknod",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, mknod);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "makedev",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, fs_makedev);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "equivalent",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, equivalent);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "file_size",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, file_size);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "hard_link_count",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, hard_link_count);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chown",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, chown);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lchown",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, lchown);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "chmod",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, chmod);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "lchmod",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, lchmod);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "read_symlink",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, read_symlink);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "remove",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, remove);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "remove_all",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, remove_all);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "rename",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, rename);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "resize_file",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, resize_file);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_empty",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_empty);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "current_working_directory",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, current_working_directory);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "exists",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, exists);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_block_file",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_block_file);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_character_file",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_character_file);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_directory",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_directory);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_fifo",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_fifo);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_other",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_other);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_regular_file",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_regular_file);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_socket",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_socket);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "is_symlink",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, is_symlink);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "space",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, space);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "status",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, status);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "symlink_status",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, symlink_status);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "temp_directory_path",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, temp_directory_path);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "umask",
            [](lua_State* L) -> int {
#if BOOST_OS_UNIX
                lua_pushcfunction(L, filesystem_umask);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_UNIX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_get_file",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, filesystem_cap_get_file);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "cap_set_file",
            [](lua_State* L) -> int {
#if BOOST_OS_LINUX
                lua_pushcfunction(L, filesystem_cap_set_file);
#else
                lua_pushcfunction(L, throw_enosys);
#endif // BOOST_OS_LINUX
                return 1;
            })
    EMILUA_GPERF_END(key)(L);
}

void init_filesystem(lua_State* L)
{
    lua_pushlightuserdata(L, &filesystem_path_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/9);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.path");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, path_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__tostring");
        lua_pushcfunction(L, path_mt_tostring);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, path_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, path_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, path_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__div");
        lua_pushcfunction(L, path_mt_div);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__concat");
        lua_pushcfunction(L, path_mt_concat);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<fs::path>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_clock_time_point_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<
            std::chrono::file_clock::time_point>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/7);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.clock.time_point");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_clock_time_point_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, file_clock_time_point_mt_eq);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__lt");
        lua_pushcfunction(L, file_clock_time_point_mt_lt);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__le");
        lua_pushcfunction(L, file_clock_time_point_mt_le);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__add");
        lua_pushcfunction(L, file_clock_time_point_mt_add);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__sub");
        lua_pushcfunction(L, file_clock_time_point_mt_sub);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &filesystem_path_iterator_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<fs::path::iterator>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &directory_iterator_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<directory_iterator>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &recursive_directory_iterator_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.recursive_directory_iterator");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, recursive_directory_iterator::mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<recursive_directory_iterator>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &space_info_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<fs::space_info>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.space_info");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, space_info_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, space_info_mt_eq);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &file_status_mt_key);
    {
        static_assert(std::is_trivially_destructible_v<fs::file_status>);

        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.file_status");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, file_status_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__eq");
        lua_pushcfunction(L, file_status_mt_eq);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &directory_entry_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.directory_entry");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, directory_entry_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<fs::directory_entry>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &path_ctors_key);
    lua_newuserdata(L, 1);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.path");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, path_ctors_mt_index);
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &clock_ctors_key);
    lua_newuserdata(L, 1);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem.clock");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, clock_ctors_mt_index);
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &filesystem_key);
    lua_newuserdata(L, 1);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/2);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "filesystem");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, filesystem_mt_index);
        lua_rawset(L, -3);
    }
    setmetatable(L, -2);
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &mode_key);
    int res = luaL_loadbuffer(
        L, reinterpret_cast<char*>(mode_bytecode), mode_bytecode_size, nullptr);
    assert(res == 0); boost::ignore_unused(res);
    lua_getglobal(L, "bit");
    lua_call(L, 1, 1);
    lua_rawset(L, LUA_REGISTRYINDEX);
}

} // namespace emilua

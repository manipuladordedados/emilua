// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/detail/landlock.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua::detail {

emilua::result<std::uint64_t, const char*>
landlock_handled_access_fs(lua_State* L)
{
    std::uint64_t flags = 0;

    for (int i = 0 ;; ++i) {
        lua_rawgeti(L, -1, i + 1);
        switch (lua_type(L, -1)) {
        case LUA_TNIL: {
            lua_pop(L, 1);
            return flags;
        }
        case LUA_TSTRING:
            break;
        default:
            return emilua::outcome::failure("invalid LANDLOCK_ACCESS_FS");
        }

        auto key = emilua::tostringview(L, -1);
        auto flag = EMILUA_GPERF_BEGIN(key)
            EMILUA_GPERF_PARAM(std::uint64_t action)
            EMILUA_GPERF_DEFAULT_VALUE(0)
            EMILUA_GPERF_PAIR("execute", 1ULL << 0)
            EMILUA_GPERF_PAIR("write_file", 1ULL << 1)
            EMILUA_GPERF_PAIR("read_file", 1ULL << 2)
            EMILUA_GPERF_PAIR("read_dir", 1ULL << 3)
            EMILUA_GPERF_PAIR("remove_dir", 1ULL << 4)
            EMILUA_GPERF_PAIR("remove_file", 1ULL << 5)
            EMILUA_GPERF_PAIR("make_char", 1ULL << 6)
            EMILUA_GPERF_PAIR("make_dir", 1ULL << 7)
            EMILUA_GPERF_PAIR("make_reg", 1ULL << 8)
            EMILUA_GPERF_PAIR("make_sock", 1ULL << 9)
            EMILUA_GPERF_PAIR("make_fifo", 1ULL << 10)
            EMILUA_GPERF_PAIR("make_block", 1ULL << 11)
            EMILUA_GPERF_PAIR("make_sym", 1ULL << 12)
            EMILUA_GPERF_PAIR("refer", 1ULL << 13)
            EMILUA_GPERF_PAIR("truncate", 1ULL << 14)
        EMILUA_GPERF_END(key);
        if (flag == 0) {
            return emilua::outcome::failure("invalid LANDLOCK_ACCESS_FS");
        }
        flags |= flag;
        lua_pop(L, 1);
    }
}

} // namespace emilua::detail

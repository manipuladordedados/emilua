#include <emilua/core.hpp>

namespace emilua {

int posix_mt_index(lua_State* L)
{
    return luaL_error(L, "key not found");
}

} // namespace emilua

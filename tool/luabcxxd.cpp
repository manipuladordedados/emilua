#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstdint>

extern "C" {
#include <lauxlib.h>
#include <lualib.h>
#include <lua.h>
}

std::string load_file(std::string path)
{
    std::string contents;
    std::ifstream in{path, std::ios::in | std::ios::binary};
    in.exceptions(std::ios_base::badbit | std::ios_base::failbit |
                  std::ios_base::eofbit);
    {
        char buf[1];
        in.read(buf, 1);
    }
    in.seekg(0, std::ios::end);
    contents.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&contents[0], contents.size());
    return contents;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << argv[0] << " <input-file> <output-file>\n";
        return 1;
    }

    const std::string id = std::filesystem::path{std::string{argv[1]}}.stem()
        .string();

    lua_State* L = luaL_newstate();
    lua_pushcfunction(L, luaopen_string);
    lua_call(L, 0, 0);

    luaL_loadstring(L, "local f = ...\nreturn string.dump(f, true)\n");
    {
        auto res = load_file(argv[1]);
        if (luaL_loadbuffer(L, res.data(), res.size(), nullptr) != 0) {
            std::cerr << "Cannot parse " << argv[1] << " as Lua:\n" <<
                lua_tostring(L, -1) << '\n';
            return 1;
        }
    }
    lua_call(L, 1, 1);

    std::size_t len;
    const std::uint8_t* buf = reinterpret_cast<const std::uint8_t*>(
        lua_tolstring(L, -1, &len));

    std::ofstream out{argv[2], std::ios::out | std::ios::trunc |
                      std::ios::binary};
    out.exceptions(std::ios_base::badbit | std::ios_base::failbit |
                   std::ios_base::eofbit);

    out <<
        "#include <cstddef>\n" <<
        "\n" <<
        "namespace emilua {\n" <<
        "unsigned char " << id << "_bytecode[] = {\n";

    if (len > 0) {
        out << "    " << unsigned{buf[0]};
    }

    for (std::size_t i = 1 ; i != len ; ++i) {
        out << ", " << unsigned{buf[i]};
    }

    out <<
        "\n};\n" <<
        "std::size_t " << id << "_bytecode_size = " << len << ";\n" <<
        "} // namespace emilua\n";
}

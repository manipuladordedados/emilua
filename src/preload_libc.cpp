// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

// include the source directly to avoid static initialization order fiasco
#include "proc_set_libc_service.cpp"

#include <charconv>
#include <cstdlib>

#include <emilua/config.h>

#include <cereal/archives/binary.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>

namespace emilua {

struct ambient_authority ambient_authority;

namespace {
static struct preload_libc
{
    preload_libc();
} preload_libc;

preload_libc::preload_libc()
{
    std::string_view env;
    if (char* e = getenv("EMILUA_LIBC_SERVICE_FD") ; e) {
        env = e;
    } else {
        return;
    }

    int fd;
    auto res = std::from_chars(env.data(), env.data() + env.size(), fd);
    if (res.ec != std::errc{} || res.ptr != env.data() + env.size())
        return;

    std::memset(const_cast<char*>(env.data()), 0, env.size());

    if (fcntl(fd, F_GETFD) == -1 && errno == EBADF)
        return;

    std::string buffer;
    buffer.resize(EMILUA_CONFIG_IPC_ACTOR_MAX_CONFIG_MESSAGE_SIZE);
    auto nread = TEMP_FAILURE_RETRY(read(fd, buffer.data(), buffer.size()));
    if (nread == -1 || nread == 0)
        return;

    buffer.resize(nread);

    std::istringstream is{buffer};
    is.imbue(std::locale::classic());
    cereal::BinaryInputArchive ia{is};

    std::map<int, std::string> lua_chunk_filters;
    ia >> lua_chunk_filters;

    libc_service::proc_set(fd, std::move(lua_chunk_filters));
}
} // namespace

} // namespace emilua

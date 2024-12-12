// Copyright (c) 2023, 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/open_posix_libs.hpp>

#include <boost/predef/os/bsd/free.h>
#include <boost/predef/os/linux.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <errno.h>

#if BOOST_OS_LINUX
#include <sys/prctl.h>
#endif // BOOST_OS_LINUX

#if BOOST_OS_BSD_FREE || BOOST_OS_BSD_DRAGONFLY
#include <sys/procctl.h>
#endif // BOOST_OS_BSD_FREE || BOOST_OS_BSD_DRAGONFLY

namespace emilua {

int posix_mt_index(lua_State* L);

static int receive_with_fd(lua_State* L)
{
    int fd = luaL_checkinteger(L, 1);
    int nbyte = luaL_checkinteger(L, 2);
    void* ud;
    lua_Alloc a = lua_getallocf(L, &ud);
    char* buf = static_cast<char*>(a(ud, NULL, 0, nbyte));

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = buf;
    iov.iov_len = nbyte;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    int res = recvmsg(fd, &msg, MSG_CMSG_CLOEXEC);
    int last_error = (res == -1) ? errno : 0;
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror("<3>ipc_actor/init");
            std::exit(1);
        }
    }
    if (last_error == 0) {
        lua_pushlstring(L, buf, res);
    } else {
        lua_pushnil(L);
    }

    int fd_received = -1;
    for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS)
            continue;

        std::memcpy(&fd_received, CMSG_DATA(cmsg), sizeof(int));
        break;
    }
    lua_pushinteger(L, fd_received);

    lua_pushinteger(L, last_error);

    return 3;
}

static int send_with_fd(lua_State* L)
{
    int fd = luaL_checkinteger(L, 1);
    std::size_t len;
    const char* str = lua_tolstring(L, 2, &len);
    int fd_to_send = luaL_checkinteger(L, 3);

    struct msghdr msg;
    std::memset(&msg, 0, sizeof(msg));

    struct iovec iov;
    iov.iov_base = const_cast<char*>(str);
    iov.iov_len = len;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    std::memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));

    int res = sendmsg(fd, &msg, MSG_NOSIGNAL);
    int last_error = (res == -1) ? errno : 0;
    if (last_error != 0) {
        lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
        if (lua_toboolean(L, -1)) {
            errno = last_error;
            perror("<3>ipc_actor/init");
            std::exit(1);
        }
    }
    lua_pushinteger(L, res);
    lua_pushinteger(L, last_error);
    return 2;
}

void open_posix_libs(lua_State* L)
{
    lua_pushboolean(L, 1);
    lua_setglobal(L, "errexit");

    lua_newtable(L);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, posix_mt_index);
        lua_rawset(L, -3);

        lua_setmetatable(L, -2);
    }
    lua_setglobal(L, "C");

    lua_pushcfunction(L, receive_with_fd);
    lua_setglobal(L, "receive_with_fd");

    lua_pushcfunction(L, send_with_fd);
    lua_setglobal(L, "send_with_fd");

    lua_pushcfunction(
        L,
        [](lua_State* L) -> int {
            mode_t u = luaL_checkinteger(L, 1);
            mode_t g = luaL_checkinteger(L, 2);
            mode_t o = luaL_checkinteger(L, 3);
            lua_pushinteger(L, (u << 6) | (g << 3) | o);
            return 1;
        });
    lua_setglobal(L, "mode");

    lua_pushcfunction(
        L,
        [](lua_State* L) -> int {
            int fd = luaL_checkinteger(L, 1);
            std::size_t len;
            const char* str = lua_tolstring(L, 2, &len);
            std::size_t nwritten = 0;
            while (nwritten < len) {
                int res = write(fd, str + nwritten, len - nwritten);
                int last_error = (res == -1) ? errno : 0;
                if (last_error != 0) {
                    lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
                    if (lua_toboolean(L, -1)) {
                        errno = last_error;
                        perror("<3>ipc_actor/init/write_all");
                        std::exit(1);
                    } else {
                        lua_pushinteger(L, nwritten);
                        lua_pushinteger(L, last_error);
                        return 2;
                    }
                }
                nwritten += res;
            }
            lua_pushinteger(L, nwritten);
            lua_pushinteger(L, 0);
            return 2;
        });
    lua_setglobal(L, "write_all");

    lua_pushcfunction(L, [](lua_State* L) -> int {
#if BOOST_OS_LINUX
        int res = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
#elif BOOST_OS_BSD_FREE
        int data = PROC_NO_NEW_PRIVS_ENABLE;
        int res = procctl(P_PID, 0, PROC_NO_NEW_PRIVS_CTL, &data);
#else
        int res = -1;
        errno = ENOSYS;
#endif // BOOST_OS_LINUX
        int last_error = (res == -1) ? errno : 0;
        if (last_error != 0) {
            lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
            if (lua_toboolean(L, -1)) {
                errno = last_error;
                perror("<3>ipc_actor/init");
                std::exit(1);
            }
        }
        lua_pushinteger(L, res);
        lua_pushinteger(L, last_error);
        return 2;
    });
    lua_setglobal(L, "set_no_new_privs");

    lua_pushcfunction(L, ([](lua_State* L) -> int {
        int fd = luaL_checkinteger(L, 1);
        std::size_t pathlen;
        const char* path = luaL_checklstring(L, 2, &pathlen);

        struct sockaddr_un addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        socklen_t addrlen = pathlen;

        // if address is a pathname (i.e. not an abstract socket address),
        // include the null byte as well for the C layer
        addrlen += (path[0] == '\0') ? 0 : 1;

        int res, last_error;
        if (addrlen > sizeof(addr.sun_path)) {
            res = -1;
            last_error = ENAMETOOLONG;
        } else {
            std::memcpy(addr.sun_path, path, addrlen);
            addrlen += offsetof(struct sockaddr_un, sun_path);
            res = bind(fd, reinterpret_cast<struct sockaddr*>(&addr), addrlen);
            last_error = (res == -1) ? errno : 0;
        }

        if (last_error != 0) {
            lua_getfield(L, LUA_GLOBALSINDEX, "errexit");
            if (lua_toboolean(L, -1)) {
                errno = last_error;
                perror("<3>ipc_actor/init");
                std::exit(1);
            }
        }
        lua_pushinteger(L, res);
        lua_pushinteger(L, last_error);
        return 2;
    }));
    lua_setglobal(L, "bind_unix");
}

} // namespace emilua

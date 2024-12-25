// Copyright (c) 2024 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/proc_set_libc_service.hpp>
#include <emilua/file_descriptor.hpp>
#include <emilua/libc_service.hpp>
#include <emilua/detail/core.hpp>
#include <emilua/async_base.hpp>
#include <emilua/filesystem.hpp>
#include <emilua/ip.hpp>

#include <boost/hana/functional/overload_linearly.hpp>
#include <boost/container/small_vector.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/scope_exit.hpp>
#include <span>

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/local/connect_pair.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/local/connect_pair.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace fs = std::filesystem;
EMILUA_GPERF_DECLS_END(includes)

namespace emilua::libc_service {

char key;
char slave_mt_key;

EMILUA_GPERF_DECLS_BEGIN(master)
EMILUA_GPERF_NAMESPACE(emilua::libc_service)
static char master_mt_key;
static char master_receive_key;
static char master_send_key;
static char master_send_with_fds_key;
static char master_use_slave_credentials_key;

struct open_request
{
    std::string path;
    int oflag;
    mode_t mode;
};

struct connect_unix_request
{
    std::string path;
};

struct connect_inet_request
{
    asio::ip::address_v4 addr;
    std::uint16_t port;
};

struct connect_inet6_request
{
    asio::ip::address_v6 addr;
    std::uint16_t port;
};

struct bind_unix_request
{
    std::string path;
};

struct bind_inet_request
{
    asio::ip::address_v4 addr;
    std::uint16_t port;
};

struct bind_inet6_request
{
    asio::ip::address_v6 addr;
    std::uint16_t port;
};

struct getaddrinfo_request
{
    std::string node;
    std::string service;
    int protocol;
};

struct master
{
    master(asio::io_context& ioctx)
        : socket{ioctx}
        , reply_buffer{std::make_shared<reply>()}
    {
        last_fds.fill(-1);
    }

    ~master()
    {
        for (int fd : last_fds) {
            if (fd != -1)
                std::ignore = close(fd);
        }
    }

    asio::local::seq_packet_protocol::socket socket;
    bool read_in_progress = false;
    std::variant<
        std::monostate,
        open_request,
        connect_unix_request,
        connect_inet_request,
        connect_inet6_request,
        bind_unix_request,
        bind_inet_request,
        bind_inet6_request,
        getaddrinfo_request
    > last_request;
    std::array<int, EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE> last_fds;
    std::shared_ptr<reply> reply_buffer;
};

struct receive_op : public std::enable_shared_from_this<receive_op>
{
    receive_op(emilua::vm_context& vm_ctx, asio::cancellation_slot cancel_slot,
               struct master& master)
        : master{master}
        , current_fiber{vm_ctx.current_fiber()}
        , vm_ctx{vm_ctx.shared_from_this()}
        , cancel_slot{std::move(cancel_slot)}
    {}

    void do_wait()
    {
        master.socket.async_wait(
            asio::socket_base::wait_read,
            asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
                vm_ctx->strand_using_defer(),
                [self=this->shared_from_this()](const asio_error_code& ec) {
                    self->on_wait(ec);
                }
            ))
        );
    }

    void on_wait(const asio_error_code& ec)
    {
        if (!vm_ctx->valid())
            return;

        if (ec) {
            master.read_in_progress = false;
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec))));
            return;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &request;
        iov.iov_len = sizeof(struct request);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        alignas(cmsghdr)
        char cmsgbuf[CMSG_SPACE(sizeof(int) * master.last_fds.size())];
        msg.msg_control = cmsgbuf;
        msg.msg_controllen = sizeof(cmsgbuf);

        auto nread = recvmsg(master.socket.native_handle(), &msg, MSG_DONTWAIT);
        if (nread == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            do_wait();
            return;
        }

        if (nread == -1) {
            master.read_in_progress = false;
            std::error_code ec2{errno, std::system_category()};
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
            return;
        }

        boost::container::small_vector<
            int, EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE> fds;
        BOOST_SCOPE_EXIT_ALL(&) {
            for (auto& fd: fds) { if (fd != -1) { std::ignore = close(fd); } }
        };
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg) ; cmsg != NULL ;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level != SOL_SOCKET ||
                cmsg->cmsg_type != SCM_RIGHTS) {
                continue;
            }

            char* in = (char*)CMSG_DATA(cmsg);
            auto nfds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (std::size_t i = 0 ; i != nfds ; ++i) {
                int fd;
                std::memcpy(&fd, in, sizeof(int));
                in += sizeof(int);
                if (fd != -1)
                    fds.emplace_back(fd);
            }
        }

        auto close_socket_and_resume_fiber_with_ebadmsg = [&]() {
            master.socket.close();
            master.read_in_progress = false;
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(
                        opt_args, hana::make_tuple(std::errc::bad_message))));
        };

        if (nread == 0) {
            master.read_in_progress = false;
            auto ec2 = make_error_code(asio::error::eof);
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
            return;
        } else if (nread != sizeof(struct request)) {
            return close_socket_and_resume_fiber_with_ebadmsg();
        }

        std::memset(master.reply_buffer.get(), 0, sizeof(struct reply));
        master.reply_buffer->id = request.id;

        assert(std::holds_alternative<std::monostate>(master.last_request));
        switch (request.function) {
        default:
            return close_socket_and_resume_fiber_with_ebadmsg();
        case request::OPEN: {
            std::string_view path{request.buffer.data(), request.buffer.size()};
            {
                auto idx = path.find('\0');
                if (idx == path.npos)
                    return close_socket_and_resume_fiber_with_ebadmsg();

                path = path.substr(0, idx);
            }

            int oflag = request.intargs[0];
            mode_t mode;

            if (
                ((oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
                ((oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
                false
            ) {
                mode = static_cast<int>(fs::perms::mask) & request.intargs[1];
            } else {
                mode = 0;
            }

            master.last_request.emplace<open_request>(
                static_cast<std::string>(path), oflag, mode);
            break;
        }
        case request::CONNECT_UNIX: {
            unsigned sz = request.uintargs[0];
            if (
                // should have at least the terminating null byte plus an extra
                // byte (otherwise we'd be dealing with an unnamed socket)
                sz < 2 ||

                sz > request.buffer.size() ||
                sz > sizeof(std::declval<struct sockaddr_un>().sun_path) ||
                fds.size() < 1
            ) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }

            std::string_view path(request.buffer.data(), sz);
            if (path.find('\0') == path.npos) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }
            if (path[0] != '\0') {
                path.remove_suffix(1);
            }
            master.last_request.emplace<connect_unix_request>(
                static_cast<std::string>(path));
            break;
        }
        case request::CONNECT_INET: {
            if (fds.size() < 1) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }

            struct sockaddr_in addr;
            std::memcpy(
                &addr, request.buffer.data(), sizeof(struct sockaddr_in));
            boost::endian::big_to_native_inplace(addr.sin_addr.s_addr);
            boost::endian::big_to_native_inplace(addr.sin_port);
            master.last_request.emplace<connect_inet_request>(
                asio::ip::address_v4{addr.sin_addr.s_addr}, addr.sin_port);
            break;
        }
        case request::CONNECT_INET6: {
            if (fds.size() < 1) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }

            struct sockaddr_in6 addr;
            std::memcpy(
                &addr, request.buffer.data(), sizeof(struct sockaddr_in6));
            asio::ip::address_v6::bytes_type bytes;
            std::memcpy(bytes.data(), &addr.sin6_addr.s6_addr[0], bytes.size());
            boost::endian::big_to_native_inplace(addr.sin6_port);
            master.last_request.emplace<connect_inet6_request>(
                asio::ip::address_v6{bytes, addr.sin6_scope_id},
                addr.sin6_port);
            break;
        }
        case request::BIND_UNIX: {
            unsigned sz = request.uintargs[0];
            if (
                // should have at least the terminating null byte plus an extra
                // byte (otherwise we'd be dealing with an unnamed socket)
                sz < 2 ||

                sz > request.buffer.size() ||
                sz > sizeof(std::declval<struct sockaddr_un>().sun_path) ||
                fds.size() < 1
            ) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }

            std::string_view path(request.buffer.data(), sz);
            if (path.find('\0') == path.npos) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }
            if (path[0] != '\0') {
                path.remove_suffix(1);
            }
            master.last_request.emplace<bind_unix_request>(
                static_cast<std::string>(path));
            break;
        }
        case request::BIND_INET: {
            if (fds.size() < 1) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }

            struct sockaddr_in addr;
            std::memcpy(
                &addr, request.buffer.data(), sizeof(struct sockaddr_in));
            boost::endian::big_to_native_inplace(addr.sin_addr.s_addr);
            boost::endian::big_to_native_inplace(addr.sin_port);
            master.last_request.emplace<bind_inet_request>(
                asio::ip::address_v4{addr.sin_addr.s_addr}, addr.sin_port);
            break;
        }
        case request::BIND_INET6: {
            if (fds.size() < 1) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }

            struct sockaddr_in6 addr;
            std::memcpy(
                &addr, request.buffer.data(), sizeof(struct sockaddr_in6));
            asio::ip::address_v6::bytes_type bytes;
            std::memcpy(bytes.data(), &addr.sin6_addr.s6_addr[0], bytes.size());
            boost::endian::big_to_native_inplace(addr.sin6_port);
            master.last_request.emplace<bind_inet6_request>(
                asio::ip::address_v6{bytes, addr.sin6_scope_id},
                addr.sin6_port);
            break;
        }
        case request::GETADDRINFO: {
            int protocol;
            switch (request.intargs[0]) {
            default:
                return close_socket_and_resume_fiber_with_ebadmsg();
            case 0:
                protocol = 0;
                break;
            case IPPROTO_TCP:
                protocol = IPPROTO_TCP;
                break;
            case IPPROTO_UDP:
                protocol = IPPROTO_UDP;
                break;
            }

            std::span<char> buffer = request.buffer;

            if (request.uintargs[0] > buffer.size()) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }
            std::string_view node{buffer.data(), request.uintargs[0]};
            buffer = buffer.last(buffer.size() - request.uintargs[0]);

            if (request.uintargs[1] > buffer.size()) {
                return close_socket_and_resume_fiber_with_ebadmsg();
            }
            std::string_view service{buffer.data(), request.uintargs[1]};
            buffer = buffer.last(buffer.size() - request.uintargs[1]);

            master.last_request.emplace<getaddrinfo_request>(
                static_cast<std::string>(node),
                static_cast<std::string>(service),
                protocol);
            break;
        }
        }

        for (int fd : master.last_fds) { assert(fd == -1); }
        auto nfds = std::min(fds.size(), master.last_fds.size());
        std::memcpy(master.last_fds.data(), fds.data(), nfds * sizeof(int));
        std::fill_n(fds.begin(), nfds, -1);

        master.read_in_progress = false;
        vm_ctx->fiber_resume(
            current_fiber,
            hana::make_set(
                vm_context::options::auto_detect_interrupt,
                hana::make_pair(opt_args, hana::make_tuple(std::nullopt))));
    }

    struct master& master;
    lua_State* current_fiber;
    std::shared_ptr<emilua::vm_context> vm_ctx;
    asio::cancellation_slot cancel_slot;
    struct request request;

    static constexpr auto opt_args = vm_context::options::arguments;
};

struct send_with_fds_op : public std::enable_shared_from_this<send_with_fds_op>
{
    struct file_descriptor_lock
    {
        file_descriptor_lock(file_descriptor_handle* reference)
            : reference{reference}
            , value{*reference}
        {}

        file_descriptor_handle* reference;
        file_descriptor_handle value;
    };

    send_with_fds_op(emilua::vm_context& vm_ctx,
                     asio::cancellation_slot cancel_slot,
                     struct master& master)
        : master{master}
        , current_fiber{vm_ctx.current_fiber()}
        , vm_ctx{vm_ctx.shared_from_this()}
        , cancel_slot{std::move(cancel_slot)}
    {
        std::memset(&reply, 0, sizeof(reply));
        reply.id = master.reply_buffer->id;
        reply.action = reply::USE_REPLY_RESULT;
    }

    void do_wait()
    {
        master.socket.async_wait(
            asio::socket_base::wait_write,
            asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
                vm_ctx->strand_using_defer(),
                [self=this->shared_from_this()](const asio_error_code& ec) {
                    self->on_wait(ec);
                }
            ))
        );
    }

    void on_wait(const asio_error_code& ec)
    {
        if (!vm_ctx->valid()) {
            for (auto& fdlock: fds) {
                std::ignore = close(fdlock.value);
            }
            return;
        }

        if (ec) {
            for (auto& fdlock: fds) {
                *fdlock.reference = fdlock.value;
            }
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec))));
            return;
        }

        struct msghdr msg;
        std::memset(&msg, 0, sizeof(msg));

        struct iovec iov;
        iov.iov_base = &reply;
        iov.iov_len = sizeof(struct reply);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        alignas(cmsghdr) char cmsgbuf[CMSG_SPACE(
            sizeof(int) * EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE)];
        if (fds.size() > 0) {
            msg.msg_control = cmsgbuf;
            msg.msg_controllen = CMSG_SPACE(sizeof(int) * fds.size());

            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());

            char* out = (char*)CMSG_DATA(cmsg);
            for (auto& fdlock : fds) {
                std::memcpy(out, &fdlock.value, sizeof(int));
                out += sizeof(int);
            }
        }

        auto nwritten = sendmsg(master.socket.native_handle(), &msg,
                                MSG_DONTWAIT | MSG_NOSIGNAL);
        if (nwritten == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            do_wait();
            return;
        }

        for (auto& fdlock: fds) {
            *fdlock.reference = fdlock.value;
        }

        if (nwritten == -1) {
            std::error_code ec2{errno, std::system_category()};
            vm_ctx->fiber_resume(
                current_fiber,
                hana::make_set(
                    vm_context::options::auto_detect_interrupt,
                    hana::make_pair(opt_args, hana::make_tuple(ec2))));
            return;
        }

        master.last_request.emplace<std::monostate>();
        for (int& fd : master.last_fds) {
            if (fd != -1) {
                std::ignore = close(fd);
                fd = -1;
            }
        }

        vm_ctx->fiber_resume(
            current_fiber,
            hana::make_set(
                vm_context::options::auto_detect_interrupt,
                hana::make_pair(opt_args, hana::make_tuple(std::nullopt))));
    }

    struct master& master;
    lua_State* current_fiber;
    std::shared_ptr<emilua::vm_context> vm_ctx;
    asio::cancellation_slot cancel_slot;
    struct reply reply;
    boost::container::small_vector<
        file_descriptor_lock, EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE> fds;

    static constexpr auto opt_args = vm_context::options::arguments;
};

static int master_receive(lua_State* L)
{
    lua_settop(L, 1);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    if (!mstr || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    if (
        !lua_rawequal(L, -1, -2) ||
        !std::holds_alternative<std::monostate>(mstr->last_request)
    ) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    if (mstr->read_in_progress) {
        push(L, std::errc::device_or_resource_busy);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    mstr->read_in_progress = true;
    auto op = std::make_shared<receive_op>(
        vm_ctx, std::move(cancel_slot), *mstr);
    op->do_wait();

    return lua_yield(L, 0);
}

template<int ERRNOARGIDX>
static std::errc fill_reply_buffer(
    lua_State* L, reply& reply_buffer,
    const decltype((std::declval<master>().last_request))& last_request
   )
{
    return std::visit(hana::overload_linearly(
        [](std::monostate) { return std::errc::invalid_argument; },
        [&](const getaddrinfo_request&) {
            switch (lua_type(L, 2)) {
            default:
                return std::errc::invalid_argument;
            case LUA_TSTRING: {
                auto strkey = tostringview(L, 2);
                auto key = EMILUA_GPERF_BEGIN(strkey)
                    EMILUA_GPERF_PARAM(int action)
                    EMILUA_GPERF_DEFAULT_VALUE(0)
                    EMILUA_GPERF_PAIR("again", EAI_AGAIN)
                    EMILUA_GPERF_PAIR("badflags", EAI_BADFLAGS)
                    EMILUA_GPERF_PAIR("fail", EAI_FAIL)
                    EMILUA_GPERF_PAIR("family", EAI_FAMILY)
                    EMILUA_GPERF_PAIR("memory", EAI_MEMORY)
                    EMILUA_GPERF_PAIR("noname", EAI_NONAME)
                    EMILUA_GPERF_PAIR("service", EAI_SERVICE)
                    EMILUA_GPERF_PAIR("socktype", EAI_SOCKTYPE)
                    EMILUA_GPERF_PAIR("system", EAI_SYSTEM)
                EMILUA_GPERF_END(strkey);
                if (key == 0) {
                    return std::errc::invalid_argument;
                }
                reply_buffer.result = key;
                break;
            }
            case LUA_TNIL: {
                reply_buffer.result = 0;
                reply_buffer.intargs[0] = AF_UNSPEC;
                break;
            }
            case LUA_TTABLE: {
                reply_buffer.result = 0;

                lua_rawgeti(L, 2, 1);
                auto a = static_cast<asio::ip::address*>(lua_touserdata(L, -1));
                if (!a || !lua_getmetatable(L, -1)) {
                    return std::errc::invalid_argument;
                }
                rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
                if (!lua_rawequal(L, -1, -2)) {
                    return std::errc::invalid_argument;
                }
                if (a->is_v4()) {
                    auto bytes = a->to_v4().to_bytes();
                    std::memcpy(
                        reply_buffer.buffer.data(), bytes.data(), bytes.size());
                    reply_buffer.intargs[0] = AF_INET;
                } else {
                    assert(a->is_v6());
                    auto as_v6 = a->to_v6();
                    auto bytes = as_v6.to_bytes();
                    std::memcpy(
                        reply_buffer.buffer.data(), bytes.data(), bytes.size());
                    reply_buffer.intargs[0] = AF_INET6;
                    reply_buffer.intargs[2] = as_v6.scope_id();
                }

                lua_rawgeti(L, 2, 2);
                switch (lua_type(L, -1)) {
                default:
                    return std::errc::invalid_argument;
                case LUA_TNIL:
                    reply_buffer.intargs[1] = 0;
                    break;
                case LUA_TNUMBER:
                    reply_buffer.intargs[1] = lua_tointeger(L, -1);
                    break;
                }
                break;
            }
            }

            switch (lua_type(L, ERRNOARGIDX)) {
            default:
                return std::errc::invalid_argument;
            case LUA_TNIL:
                reply_buffer.error_code = 0;
                break;
            case LUA_TNUMBER:
                reply_buffer.error_code = lua_tointeger(L, ERRNOARGIDX);
                break;
            case LUA_TTABLE: {
                if (!lua_getmetatable(L, ERRNOARGIDX))
                    return std::errc::invalid_argument;
                rawgetp(L, LUA_REGISTRYINDEX,
                        &emilua::detail::error_code_mt_key);
                if (!lua_rawequal(L, -1, -2))
                    return std::errc::invalid_argument;
                lua_pushliteral(L, "category");
                lua_rawget(L, ERRNOARGIDX);
                auto cat = static_cast<const std::error_category**>(
                    lua_touserdata(L, -1));
                if (!lua_getmetatable(L, -1))
                    return std::errc::invalid_argument;
                rawgetp(L, LUA_REGISTRYINDEX,
                        &emilua::detail::error_category_mt_key);
                if (!lua_rawequal(L, -1, -2))
                    return std::errc::invalid_argument;
                if (*cat != &std::generic_category() &&
                    *cat != &std::system_category()) {
                    return std::errc::invalid_argument;
                }
                lua_pushliteral(L, "code");
                lua_rawget(L, ERRNOARGIDX);
                if (lua_type(L, -1) != LUA_TNUMBER)
                    return std::errc::invalid_argument;
                reply_buffer.error_code = lua_tointeger(L, -1);
                break;
            }
            }
            return std::errc{};
        },
        [&](const auto&) {
            reply_buffer.result = luaL_checkinteger(L, 2);
            switch (lua_type(L, ERRNOARGIDX)) {
            default:
                return std::errc::invalid_argument;
            case LUA_TNIL:
                reply_buffer.error_code = 0;
                break;
            case LUA_TNUMBER:
                reply_buffer.error_code = lua_tointeger(L, ERRNOARGIDX);
                break;
            case LUA_TTABLE: {
                if (!lua_getmetatable(L, ERRNOARGIDX))
                    return std::errc::invalid_argument;
                rawgetp(L, LUA_REGISTRYINDEX,
                        &emilua::detail::error_code_mt_key);
                if (!lua_rawequal(L, -1, -2))
                    return std::errc::invalid_argument;
                lua_pushliteral(L, "category");
                lua_rawget(L, ERRNOARGIDX);
                auto cat = static_cast<const std::error_category**>(
                    lua_touserdata(L, -1));
                if (!lua_getmetatable(L, -1))
                    return std::errc::invalid_argument;
                rawgetp(L, LUA_REGISTRYINDEX,
                        &emilua::detail::error_category_mt_key);
                if (!lua_rawequal(L, -1, -2))
                    return std::errc::invalid_argument;
                if (*cat != &std::generic_category() &&
                    *cat != &std::system_category()) {
                    return std::errc::invalid_argument;
                }
                lua_pushliteral(L, "code");
                lua_rawget(L, ERRNOARGIDX);
                if (lua_type(L, -1) != LUA_TNUMBER)
                    return std::errc::invalid_argument;
                reply_buffer.error_code = lua_tointeger(L, -1);
                break;
            }
            }
            return std::errc{};
        }
    ), last_request);
}

static int master_send(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    if (!mstr || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    mstr->reply_buffer->action = reply::USE_REPLY_RESULT;
    std::errc e = fill_reply_buffer</*ERRNOARGIDX=*/3>(
        L, *mstr->reply_buffer, mstr->last_request);
    if (e != std::errc{}) {
        push(L, e);
        return lua_error(L);
    }

    auto buf = mstr->reply_buffer;
    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    mstr->socket.async_send(
        asio::buffer(buf.get(), sizeof(struct reply)), /*flags=*/0,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,mstr,buf](
                const asio_error_code& ec, std::size_t /*bytes_transferred*/
            ) {
                auto opt_args = vm_context::options::arguments;
                if (!vm_ctx->valid())
                    return;

                if (ec) {
                    vm_ctx->fiber_resume(
                        current_fiber,
                        hana::make_set(
                            vm_context::options::auto_detect_interrupt,
                            hana::make_pair(opt_args, hana::make_tuple(ec))));
                    return;
                }

                mstr->last_request.emplace<std::monostate>();
                for (int& fd : mstr->last_fds) {
                    if (fd != -1) {
                        std::ignore = close(fd);
                        fd = -1;
                    }
                }

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int master_send_with_fds(lua_State* L)
{
    lua_settop(L, 4);

    auto& vm_ctx = get_vm_context(L);
    EMILUA_CHECK_SUSPEND_ALLOWED(vm_ctx, L);

    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    if (!mstr || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    auto cancel_slot = set_default_interrupter(L, vm_ctx);

    auto op = std::make_shared<send_with_fds_op>(
        vm_ctx, std::move(cancel_slot), *mstr);

    std::errc e = fill_reply_buffer</*ERRNOARGIDX=*/4>(
        L, op->reply, mstr->last_request);
    if (e != std::errc{}) {
        push(L, e);
        return lua_error(L);
    }

    rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
    for (int i = 1 ;; ++i) {
        lua_rawgeti(L, 3, i);
        auto current_element_type = lua_type(L, -1);
        if (current_element_type == LUA_TNIL)
            break;

        switch (current_element_type) {
        default:
            assert(current_element_type != LUA_TNIL);
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        case LUA_TUSERDATA:
            break;
        }

        auto handle = static_cast<file_descriptor_handle*>(
            lua_touserdata(L, -1));
        if (!lua_getmetatable(L, -1)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
        if (!lua_rawequal(L, -1, -3)) {
            push(L, std::errc::invalid_argument, "arg", 3);
            return lua_error(L);
        }
        if (*handle == INVALID_FILE_DESCRIPTOR) {
            push(L, std::errc::device_or_resource_busy);
            return lua_error(L);
        }

        bool found_in_the_set = false;
        for (auto& fdlock: op->fds) {
            if (fdlock.reference == handle) {
                found_in_the_set = true;
                break;
            }
        }

        if (!found_in_the_set)
            op->fds.emplace_back(handle);
        lua_pop(L, 2);
    }

    if (op->fds.size() > EMILUA_LIBC_SERVICE_MAXIMUM_FDS_PER_MESSAGE) {
        push(L, std::errc::value_too_large, "arg", 3);
        return lua_error(L);
    }

    for (auto& fdlock: op->fds) {
        *fdlock.reference = INVALID_FILE_DESCRIPTOR;
    }
    op->do_wait();

    return lua_yield(L, 0);
}

static int master_use_slave_credentials(lua_State* L)
{
    lua_settop(L, 3);

    auto vm_ctx = get_vm_context(L).shared_from_this();
    auto current_fiber = vm_ctx->current_fiber();
    EMILUA_CHECK_SUSPEND_ALLOWED(*vm_ctx, L);

    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    if (!mstr || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    if (
        !lua_rawequal(L, -1, -2) ||
        std::holds_alternative<std::monostate>(mstr->last_request)
    ) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    mstr->reply_buffer->action = reply::FORWARD_TO_REAL_LIBC;

    auto buf = mstr->reply_buffer;
    auto cancel_slot = set_default_interrupter(L, *vm_ctx);

    mstr->socket.async_send(
        asio::buffer(buf.get(), sizeof(struct reply)), /*flags=*/0,
        asio::bind_cancellation_slot(cancel_slot, asio::bind_executor(
            vm_ctx->strand_using_defer(),
            [vm_ctx,current_fiber,mstr,buf](
                const asio_error_code& ec, std::size_t /*bytes_transferred*/
            ) {
                auto opt_args = vm_context::options::arguments;
                if (!vm_ctx->valid())
                    return;

                if (ec) {
                    vm_ctx->fiber_resume(
                        current_fiber,
                        hana::make_set(
                            vm_context::options::auto_detect_interrupt,
                            hana::make_pair(opt_args, hana::make_tuple(ec))));
                    return;
                }

                mstr->last_request.emplace<std::monostate>();
                for (int& fd : mstr->last_fds) {
                    if (fd != -1) {
                        std::ignore = close(fd);
                        fd = -1;
                    }
                }

                vm_ctx->fiber_resume(
                    current_fiber,
                    hana::make_set(
                        vm_context::options::auto_detect_interrupt,
                        hana::make_pair(opt_args, hana::make_tuple(ec))));
            }
        ))
    );

    return lua_yield(L, 0);
}

static int master_arguments(lua_State* L)
{
    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    if (!mstr || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    return std::visit(hana::overload(
        [&](std::monostate) {
            push(L, std::errc::no_message);
            return lua_error(L);
        },
        [&](const open_request& r) {
            auto p = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (p) fs::path{};
            *p = fs::path{r.path, fs::path::native_format};

            lua_newtable(L);
            {
                int i = 1;

                if ((r.oflag & O_APPEND) == O_APPEND) {
                    lua_pushliteral(L, "append");
                    lua_rawseti(L, -2, i++);
                }

                if ((r.oflag & O_CREAT) == O_CREAT) {
                    lua_pushliteral(L, "create");
                    lua_rawseti(L, -2, i++);
                }

                if ((r.oflag & O_EXCL) == O_EXCL) {
                    lua_pushliteral(L, "exclusive");
                    lua_rawseti(L, -2, i++);
                }

                if ((r.oflag & O_RDWR) == O_RDWR) {
                    lua_pushliteral(L, "read_write");
                    lua_rawseti(L, -2, i++);
                } else if ((r.oflag & O_WRONLY) == O_WRONLY) {
                    lua_pushliteral(L, "write_only");
                    lua_rawseti(L, -2, i++);
                } else if ((r.oflag & O_RDONLY) == O_RDONLY) {
                    lua_pushliteral(L, "read_only");
                    lua_rawseti(L, -2, i++);
                }

                if ((r.oflag & O_SYNC) == O_SYNC) {
                    lua_pushliteral(L, "sync_all_on_write");
                    lua_rawseti(L, -2, i++);
                }

                if ((r.oflag & O_TRUNC) == O_TRUNC) {
                    lua_pushliteral(L, "truncate");
                    lua_rawseti(L, -2, i++);
                }

#ifdef O_TMPFILE
                if ((r.oflag & O_TMPFILE) == O_TMPFILE) {
                    lua_pushliteral(L, "temporary");
                    lua_rawseti(L, -2, i++);
                } else if ((r.oflag & O_DIRECTORY) == O_DIRECTORY) {
                    lua_pushliteral(L, "directory");
                    lua_rawseti(L, -2, i++);
                }
#else // defined(O_TMPFILE)
                if ((r.oflag & O_DIRECTORY) == O_DIRECTORY) {
                    lua_pushliteral(L, "directory");
                    lua_rawseti(L, -2, i++);
                }
#endif // defined(O_TMPFILE)
            }

            if (
                ((r.oflag & O_CREAT) == O_CREAT) ||
#ifdef O_TMPFILE
                ((r.oflag & O_TMPFILE) == O_TMPFILE) ||
#endif // defined(O_TMPFILE)
                false
            ) {
                lua_pushinteger(L, r.mode);
                return 3;
            } else {
                return 2;
            }
        },
        [&](const connect_unix_request& r) {
            auto p = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (p) fs::path{};
            *p = fs::path{r.path, fs::path::native_format};
            return 1;
        },
        [&](const connect_inet_request& r) {
            auto addr = static_cast<asio::ip::address*>(
                lua_newuserdata(L, sizeof(asio::ip::address))
            );
            rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
            setmetatable(L, -2);
            new (addr) asio::ip::address{r.addr};

            lua_pushinteger(L, r.port);

            return 2;
        },
        [&](const connect_inet6_request& r) {
            auto addr = static_cast<asio::ip::address*>(
                lua_newuserdata(L, sizeof(asio::ip::address))
            );
            rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
            setmetatable(L, -2);
            new (addr) asio::ip::address{r.addr};

            lua_pushinteger(L, r.port);

            return 2;
        },
        [&](const bind_unix_request& r) {
            auto p = static_cast<fs::path*>(
                lua_newuserdata(L, sizeof(fs::path)));
            rawgetp(L, LUA_REGISTRYINDEX, &filesystem_path_mt_key);
            setmetatable(L, -2);
            new (p) fs::path{};
            *p = fs::path{r.path, fs::path::native_format};
            return 1;
        },
        [&](const bind_inet_request& r) {
            auto addr = static_cast<asio::ip::address*>(
                lua_newuserdata(L, sizeof(asio::ip::address))
            );
            rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
            setmetatable(L, -2);
            new (addr) asio::ip::address{r.addr};

            lua_pushinteger(L, r.port);

            return 2;
        },
        [&](const bind_inet6_request& r) {
            auto addr = static_cast<asio::ip::address*>(
                lua_newuserdata(L, sizeof(asio::ip::address))
            );
            rawgetp(L, LUA_REGISTRYINDEX, &ip_address_mt_key);
            setmetatable(L, -2);
            new (addr) asio::ip::address{r.addr};

            lua_pushinteger(L, r.port);

            return 2;
        },
        [&](const getaddrinfo_request& r) {
            push(L, r.node);
            push(L, r.service);
            switch (r.protocol) {
            case 0:
                lua_pushnil(L);
                break;
            case IPPROTO_TCP:
                lua_pushliteral(L, "tcp");
                break;
            case IPPROTO_UDP:
                lua_pushliteral(L, "udp");
                break;
            default:
                assert(false);
            }
            return 3;
        }
    ), mstr->last_request);
}

static int master_descriptors(lua_State* L)
{
    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    if (!mstr || !lua_getmetatable(L, 1)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    if (!lua_rawequal(L, -1, -2)) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    }

    // not every call takes a file descriptor as argument
    static constexpr auto einval = [](lua_State* L) {
        push(L, std::errc::invalid_argument, "arg", 1);
        return lua_error(L);
    };

    return std::visit(hana::overload(
        [&](std::monostate) {
            push(L, std::errc::no_message);
            return lua_error(L);
        },
        [&](const open_request& r) { return einval(L); },
        [&](const connect_unix_request& r) {
            if (mstr->last_fds[0] == -1) {
                lua_pushnil(L);
                return 1;
            } else {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(L, -2);
                *fdhandle = mstr->last_fds[0];
                mstr->last_fds[0] = -1;
                return 1;
            }
        },
        [&](const connect_inet_request& r) {
            if (mstr->last_fds[0] == -1) {
                lua_pushnil(L);
                return 1;
            } else {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(L, -2);
                *fdhandle = mstr->last_fds[0];
                mstr->last_fds[0] = -1;
                return 1;
            }
        },
        [&](const connect_inet6_request& r) {
            if (mstr->last_fds[0] == -1) {
                lua_pushnil(L);
                return 1;
            } else {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(L, -2);
                *fdhandle = mstr->last_fds[0];
                mstr->last_fds[0] = -1;
                return 1;
            }
        },
        [&](const bind_unix_request& r) {
            if (mstr->last_fds[0] == -1) {
                lua_pushnil(L);
                return 1;
            } else {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(L, -2);
                *fdhandle = mstr->last_fds[0];
                mstr->last_fds[0] = -1;
                return 1;
            }
        },
        [&](const bind_inet_request& r) {
            if (mstr->last_fds[0] == -1) {
                lua_pushnil(L);
                return 1;
            } else {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(L, -2);
                *fdhandle = mstr->last_fds[0];
                mstr->last_fds[0] = -1;
                return 1;
            }
        },
        [&](const bind_inet6_request& r) {
            if (mstr->last_fds[0] == -1) {
                lua_pushnil(L);
                return 1;
            } else {
                auto fdhandle = static_cast<file_descriptor_handle*>(
                    lua_newuserdata(L, sizeof(file_descriptor_handle))
                );
                rawgetp(L, LUA_REGISTRYINDEX, &file_descriptor_mt_key);
                setmetatable(L, -2);
                *fdhandle = mstr->last_fds[0];
                mstr->last_fds[0] = -1;
                return 1;
            }
        },
        [&](const getaddrinfo_request& r) { return einval(L); }
    ), mstr->last_request);
}

inline int master_function_(lua_State* L)
{
    auto mstr = static_cast<master*>(lua_touserdata(L, 1));
    return std::visit(hana::overload(
        [&](std::monostate) {
            push(L, std::errc::no_message);
            return lua_error(L);
        },
        [&](const open_request&) {
            lua_pushliteral(L, "open");
            return 1;
        },
        [&](const connect_unix_request&) {
            lua_pushliteral(L, "connect_unix");
            return 1;
        },
        [&](const connect_inet_request&) {
            lua_pushliteral(L, "connect_inet");
            return 1;
        },
        [&](const connect_inet6_request&) {
            lua_pushliteral(L, "connect_inet6");
            return 1;
        },
        [&](const bind_unix_request&) {
            lua_pushliteral(L, "bind_unix");
            return 1;
        },
        [&](const bind_inet_request&) {
            lua_pushliteral(L, "bind_inet");
            return 1;
        },
        [&](const bind_inet6_request&) {
            lua_pushliteral(L, "bind_inet6");
            return 1;
        },
        [&](const getaddrinfo_request&) {
            lua_pushliteral(L, "getaddrinfo");
            return 1;
        }
    ), mstr->last_request);
}
EMILUA_GPERF_DECLS_END(master)

static int master_mt_index(lua_State* L)
{
    auto key = tostringview(L, 2);
    return EMILUA_GPERF_BEGIN(key)
        EMILUA_GPERF_PARAM(int (*action)(lua_State*))
        EMILUA_GPERF_DEFAULT_VALUE([](lua_State* L) -> int {
            push(L, errc::bad_index, "index", 2);
            return lua_error(L);
        })
        EMILUA_GPERF_PAIR(
            "receive",
            [](lua_State* L) -> int {
                rawgetp(
                    L, LUA_REGISTRYINDEX, &libc_service::master_receive_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send",
            [](lua_State* L) -> int {
                rawgetp(
                    L, LUA_REGISTRYINDEX, &libc_service::master_send_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "send_with_fds",
            [](lua_State* L) -> int {
                rawgetp(
                    L, LUA_REGISTRYINDEX,
                    &libc_service::master_send_with_fds_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "use_slave_credentials",
            [](lua_State* L) -> int {
                rawgetp(
                    L, LUA_REGISTRYINDEX,
                    &libc_service::master_use_slave_credentials_key);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "arguments",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, libc_service::master_arguments);
                return 1;
            })
        EMILUA_GPERF_PAIR(
            "descriptors",
            [](lua_State* L) -> int {
                lua_pushcfunction(L, libc_service::master_descriptors);
                return 1;
            })
        EMILUA_GPERF_PAIR("function_", libc_service::master_function_)
    EMILUA_GPERF_END(key)(L);
}

static int slave_mt_newindex(lua_State* L)
{
    if (lua_type(L, 2) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    if (lua_type(L, 3) != LUA_TSTRING) {
        push(L, std::errc::invalid_argument, "arg", 3);
        return lua_error(L);
    }

    auto slv = static_cast<slave*>(lua_touserdata(L, 1));
    auto strkey = tostringview(L, 2);
    auto value = tostringview(L, 3);

    auto key = EMILUA_GPERF_BEGIN(strkey)
        EMILUA_GPERF_PARAM(int action)
        EMILUA_GPERF_DEFAULT_VALUE(-1)
        EMILUA_GPERF_PAIR("open", libc_service::request::OPEN)
        EMILUA_GPERF_PAIR("connect_unix", libc_service::request::CONNECT_UNIX)
        EMILUA_GPERF_PAIR("connect_inet", libc_service::request::CONNECT_INET)
        EMILUA_GPERF_PAIR("connect_inet6", libc_service::request::CONNECT_INET6)
        EMILUA_GPERF_PAIR("bind_unix", libc_service::request::BIND_UNIX)
        EMILUA_GPERF_PAIR("bind_inet", libc_service::request::BIND_INET)
        EMILUA_GPERF_PAIR("bind_inet6", libc_service::request::BIND_INET6)
        EMILUA_GPERF_PAIR("getaddrinfo", libc_service::request::GETADDRINFO)
    EMILUA_GPERF_END(strkey);

    if (key == -1) {
        push(L, std::errc::invalid_argument, "arg", 2);
        return lua_error(L);
    }

    slv->lua_chunk_filters[key] = value;
    return 0;
}

static int new_(lua_State* L)
{
    auto& vm_ctx = get_vm_context(L);

    auto mstr = static_cast<master*>(lua_newuserdata(L, sizeof(master)));
    rawgetp(L, LUA_REGISTRYINDEX, &master_mt_key);
    setmetatable(L, -2);
    new (mstr) master{vm_ctx.strand().context()};

    auto slv = static_cast<slave*>(lua_newuserdata(L, sizeof(slave)));
    rawgetp(L, LUA_REGISTRYINDEX, &slave_mt_key);
    setmetatable(L, -2);
    new (slv) slave{vm_ctx.strand().context()};

    asio_error_code ec;
    asio::local::connect_pair(mstr->socket, slv->socket, ec);
    if (ec) {
        push(L, static_cast<std::error_code>(ec));
        return lua_error(L);
    }

    slv->masterdupfd = dup(mstr->socket.native_handle());
    if (slv->masterdupfd == -1) {
        push(L, std::error_code{errno, std::system_category()});
        return lua_error(L);
    }

    return 2;
}

void init(lua_State* L)
{
    lua_pushlightuserdata(L, &master_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "libc_service.master");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, master_mt_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<master>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &slave_mt_key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/3);

        lua_pushliteral(L, "__metatable");
        lua_pushliteral(L, "libc_service.slave");
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, slave_mt_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__gc");
        lua_pushcfunction(L, finalizer<slave>);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    lua_pushlightuserdata(L, &key);
    {
        lua_createtable(L, /*narr=*/0, /*nrec=*/1);

        lua_pushliteral(L, "new");
        lua_pushcfunction(L, new_);
        lua_rawset(L, -3);
    }
    lua_rawset(L, LUA_REGISTRYINDEX);

    {
        lua_pushlightuserdata(L, &master_receive_key);
        rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, master_receive);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
    {
        lua_pushlightuserdata(L, &master_send_key);
        rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, master_send);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
    {
        lua_pushlightuserdata(L, &master_send_with_fds_key);
        rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, master_send_with_fds);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
    {
        lua_pushlightuserdata(L, &master_use_slave_credentials_key);
        rawgetp(L, LUA_REGISTRYINDEX, &var_args__retval1_to_error__key);
        rawgetp(L, LUA_REGISTRYINDEX, &raw_error_key);
        lua_pushcfunction(L, master_use_slave_credentials);
        lua_call(L, 2, 1);
        lua_rawset(L, LUA_REGISTRYINDEX);
    }
}

} // namespace emilua::libc_service

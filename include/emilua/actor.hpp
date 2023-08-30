/* Copyright (c) 2020, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

#if BOOST_OS_LINUX
#include <sys/capability.h>
#include <sys/syscall.h>
#endif // BOOST_OS_LINUX

#if BOOST_OS_UNIX
#include <boost/asio/local/datagram_protocol.hpp>
#endif // BOOST_OS_UNIX

namespace emilua {

extern char inbox_key;

void init_actor_module(lua_State* L);

#if BOOST_OS_UNIX
static constexpr std::uint64_t DOUBLE_SIGN_BIT = UINT64_C(0x8000000000000000);
static constexpr std::uint64_t EXPONENT_MASK   = UINT64_C(0x7FF0000000000000);
static constexpr std::uint64_t MANTISSA_MASK   = UINT64_C(0x000FFFFFFFFFFFFF);
static constexpr std::uint64_t QNAN_BIT        = UINT64_C(0x0008000000000000);

extern char ipc_actor_chan_mt_key;

// If members[0]'s type is nil then it means the message is flat (i.e. a sole
// root non-composite value) and its value is that of members[1].
struct ipc_actor_message
{
    enum kind : std::uint64_t
    {
        boolean_true    = 1,
        boolean_false   = 2,
        string          = 3,
        file_descriptor = 4,
        actor_address   = 5,
        nil             = 6
    };

    union {
        double as_double;
        std::uint64_t as_int;
    } members[EMILUA_CONFIG_IPC_ACTOR_MESSAGE_MAX_MEMBERS_NUMBER];
    unsigned char strbuf[
        EMILUA_CONFIG_IPC_ACTOR_MESSAGE_SIZE - sizeof(members)];
};
static_assert(sizeof(ipc_actor_message) ==
              EMILUA_CONFIG_IPC_ACTOR_MESSAGE_SIZE);
static_assert(EMILUA_CONFIG_IPC_ACTOR_MESSAGE_MAX_MEMBERS_NUMBER *
              512 == sizeof(std::declval<ipc_actor_message>().strbuf));
static_assert(EMILUA_CONFIG_IPC_ACTOR_MESSAGE_MAX_MEMBERS_NUMBER > 2);

struct ipc_actor_inbox_service;

struct ipc_actor_inbox_op
    : public std::enable_shared_from_this<ipc_actor_inbox_op>
{
    ipc_actor_inbox_op(vm_context& vm_ctx, ipc_actor_inbox_service* service)
        : executor{vm_ctx.strand()}
        , vm_ctx{vm_ctx.weak_from_this()}
        , service{service}
    {}

    void do_wait();
    void on_wait(const boost::system::error_code& ec);

private:
    strand_type executor;
    std::weak_ptr<vm_context> vm_ctx;
    ipc_actor_inbox_service* service;
};

struct ipc_actor_inbox_service : public pending_operation
{
    ipc_actor_inbox_service(asio::io_context& ioctx, int inboxfd)
        : pending_operation{/*shared_ownership=*/false}
        , sock{ioctx}
    {
        asio::local::datagram_protocol protocol;
        boost::system::error_code ignored_ec;
        sock.assign(protocol, inboxfd, ignored_ec);
        assert(!ignored_ec);
    }

    void async_enqueue(vm_context& vm_ctx)
    {
        if (running)
            return;

        running = true;
        auto op = std::make_shared<ipc_actor_inbox_op>(vm_ctx, this);
        op->do_wait();
    }

    void cancel() noexcept override
    {}

    asio::local::datagram_protocol::socket sock;
    bool running = false;
};

struct bzero_region
{
    void *s;
    size_t n;
};

struct ipc_actor_start_vm_request
{
    enum action : std::uint8_t
    {
        CLOSE_FD,
        SHARE_PARENT,
        USE_PIPE
    };

    enum : std::uint8_t
    {
        CREATE_PROCESS,
        SETRESUID,
        SETRESGID,
        SETGROUPS,
        CAP_SET_PROC,
        CAP_DROP_BOUND,
        CAP_SET_AMBIENT,
        CAP_RESET_AMBIENT,
        CAP_SET_SECBITS
    } type;

    int clone_flags;
    action stdin_action;
    action stdout_action;
    action stderr_action;
    std::uint8_t stderr_has_color;
    std::uint8_t has_lua_hook;

    uid_t resuid[3];
    gid_t resgid[3];
    int setgroups_ngroups;
    ssize_t cap_set_proc_mfd_size;
    cap_value_t cap_value;
    cap_flag_value_t cap_flag_value;
    unsigned cap_set_secbits_value;
};

struct ipc_actor_start_vm_reply
{
    pid_t childpid;
    int error;
};

inline bool is_snan(std::uint64_t as_i)
{
  return (as_i & EXPONENT_MASK) == EXPONENT_MASK &&
      (as_i & MANTISSA_MASK) != 0 &&
      (as_i & QNAN_BIT) == 0;
}

struct ipc_actor_reaper : public pending_operation
{
    ipc_actor_reaper(int childpidfd, pid_t childpid)
        : pending_operation{/*shared_ownership=*/false}
#if BOOST_OS_LINUX
        , childpidfd{childpidfd}
#endif // BOOST_OS_LINUX
        , childpid{childpid}
    {}

    ~ipc_actor_reaper()
    {
#if BOOST_OS_LINUX
        close(childpidfd);
#else // BOOST_OS_LINUX
        // TODO
#endif // BOOST_OS_LINUX
    }

    void cancel() noexcept override
    {
#if BOOST_OS_LINUX
        syscall(SYS_pidfd_send_signal, childpidfd, SIGKILL, /*info=*/NULL,
                /*flags=*/0);
#else // BOOST_OS_LINUX
        // TODO
#endif // BOOST_OS_LINUX
    }

#if BOOST_OS_LINUX
    int childpidfd;
#endif // BOOST_OS_LINUX
    pid_t childpid;
};

struct ipc_actor_address
{
    ipc_actor_address(asio::io_context& ioctx)
        : dest{ioctx}
    {}

    asio::local::datagram_protocol::socket dest;
    ipc_actor_reaper* reaper = nullptr;
};
#endif // BOOST_OS_UNIX

} // namespace emilua

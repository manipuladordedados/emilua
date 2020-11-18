#pragma once

#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/intrusive/list.hpp>

#include <boost/outcome/basic_result.hpp>
#include <boost/outcome/policy/all_narrow.hpp>
#include <boost/outcome/policy/terminate.hpp>

#include <system_error>
#include <string_view>
#include <filesystem>
#include <variant>
#include <atomic>
#include <mutex>

extern "C" {
#include <lauxlib.h>
#include <luajit.h>
#include <lualib.h>
#include <lua.h>
}

#include <config.h>

#define EMILUA_CHECK_SUSPEND_ALLOWED(VM_CTX, L)             \
    if (!emilua::detail::unsafe_can_suspend((VM_CTX), (L))) \
        return lua_error((L));

#define EMILUA_CHECK_SUSPEND_ALLOWED_ASSUMING_INTERRUPTION_DISABLED(VM_CTX, L) \
    if (!emilua::detail::unsafe_can_suspend2((VM_CTX), (L)))                   \
        return lua_error((L));

namespace boost::hana {}

namespace emilua {

using namespace std::literals::string_view_literals;
namespace outcome = BOOST_OUTCOME_V2_NAMESPACE;
namespace asio = boost::asio;
namespace hana = boost::hana;

extern bool stdout_has_color;
extern char raw_unpack_key;
extern char raw_xpcall_key;
extern char raw_pcall_key;

template<class T, class EC = std::error_code>
using result = outcome::basic_result<
    T, EC,
#ifdef NDEBUG
    outcome::policy::all_narrow
#else
    outcome::policy::terminate
#endif // defined(NDEBUG)
>;

namespace detail {
template<class Executor>
class remap_post_to_defer: private Executor
{
public:
    remap_post_to_defer(const remap_post_to_defer&) = default;
    remap_post_to_defer(remap_post_to_defer&&) = default;

    explicit remap_post_to_defer(const Executor& ex)
        : Executor(ex)
    {}

    explicit remap_post_to_defer(Executor&& ex)
        : Executor(std::move(ex))
    {}

    bool operator==(const remap_post_to_defer& o) const noexcept
    {
        return static_cast<const Executor&>(*this) ==
            static_cast<const Executor&>(o);
    }

    bool operator!=(const remap_post_to_defer& o) const noexcept
    {
        return static_cast<const Executor&>(*this) !=
            static_cast<const Executor&>(o);
    }

    decltype(std::declval<Executor>().context())
    context() const noexcept
    {
        return Executor::context();
    }

    void on_work_started() const noexcept
    {
        Executor::on_work_started();
    }

    void on_work_finished() const noexcept
    {
        Executor::on_work_finished();
    }

    template<class F, class A>
    void dispatch(F&& f, const A& a) const
    {
        Executor::dispatch(std::forward<F>(f), a);
    }

    template<class F, class A>
    void post(F&& f, const A& a) const
    {
        Executor::defer(std::forward<F>(f), a);
    }

    template<class F, class A>
    void defer(F&& f, const A& a) const
    {
        Executor::defer(std::forward<F>(f), a);
    }
};
} // namespace detail

class service: public boost::asio::io_context::service
{
private:
    struct path_hash
    {
        std::size_t operator()(const std::filesystem::path& p) const noexcept
        {
            return std::filesystem::hash_value(p);
        }
    };

public:
    using key_type = service;
    explicit service(boost::asio::io_context& ctx)
        : boost::asio::io_context::service(ctx)
    {}

    std::unordered_map<std::filesystem::path, std::string, path_hash>
        modules_cache_registry;
    std::mutex modules_cache_registry_mtx;

    static boost::asio::io_context::id id;
};

class dead_vm_error: public std::runtime_error
{
public:
    enum class reason
    {
        unknown,
        mem,
    };

    dead_vm_error()
        : std::runtime_error{""}
        , code{0}
    {}

    dead_vm_error(reason r)
        : std::runtime_error{nullptr}
        , code{static_cast<int>(r)}
    {}

    virtual const char* what() const noexcept override
    {
        static const char* reasons[] = {
            "Lua VM is dead",
            "Lua VM is dead due to LUA_ERRMEM"
        };
        return reasons[code];
    }

private:
    int code;
};

void set_interrupter(lua_State* L);

class vm_context;

struct actor_address
{
    actor_address(vm_context& vm_ctx);
    ~actor_address();

    actor_address(actor_address&&) = default;
    actor_address(const actor_address&);

    actor_address& operator=(actor_address&& o);

    std::weak_ptr<vm_context> dest;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard;
};

struct inbox_t
{
    struct value_type: std::variant<
        bool, lua_Number, std::string,
        std::map<std::string, value_type>,
        std::vector<value_type>,
        actor_address
    >
    {
        using variant_type = variant;

        using variant::variant;

        value_type(value_type&&) = default;
        value_type(const value_type&) = default;

        value_type& operator=(value_type&&) = default;
    };

    struct sender_state
    {
        sender_state(vm_context& vm_ctx);
        sender_state(vm_context& vm_ctx, lua_State* fiber);
        sender_state(sender_state&& o);

        ~sender_state();

        sender_state& operator=(sender_state&& o);

        sender_state(const sender_state&) = delete;
        sender_state& operator=(const sender_state&) = delete;

        // check whether is same sender
        bool operator==(const sender_state& o)
        {
            // `msg` is ignored
            return vm_ctx == o.vm_ctx && fiber == o.fiber;
        }

        std::shared_ptr<vm_context> vm_ctx;
        asio::executor_work_guard<asio::io_context::executor_type> work_guard;
        lua_State* fiber;
        value_type msg;
        bool wake_on_destruct = false;
    };

    lua_State* recv_fiber = nullptr;
    std::deque<sender_state> incoming;
    bool open = true;
    bool imported = false;
    std::atomic_size_t nsenders = 0;
    std::shared_ptr<vm_context> work_guard;
};

// This class represents a node to be destroyed when the VM finishes
// prematurely. It can be used to register cleanup code (the `cancel()` method).
class pending_operation
    : public boost::intrusive::list_base_hook<
        boost::intrusive::link_mode<boost::intrusive::auto_unlink>
    >
{
public:
    pending_operation(bool shared_ownership)
        : shared_ownership(shared_ownership)
    {}

    virtual ~pending_operation() noexcept = default;

    virtual void cancel() noexcept = 0;

    // If `shared_ownership`, then the runtime won't `delete` the node after it
    // is removed from the list of pending operations. It's useful if you don't
    // want to allocate `pending_operation` on the heap (and other scenarios).
    bool shared_ownership;
};

class vm_context: public std::enable_shared_from_this<vm_context>
{
public:
    vm_context(boost::asio::io_context::strand strand);
    ~vm_context();

    vm_context(const vm_context&) = delete;
    vm_context(vm_context&&) = delete;

    vm_context& operator=(const vm_context&) = delete;
    vm_context& operator=(vm_context&&) = delete;

    const boost::asio::io_context::strand& strand()
    {
        return strand_;
    }

    detail::remap_post_to_defer<boost::asio::io_context::strand>
    strand_using_defer()
    {
        return detail::remap_post_to_defer<boost::asio::io_context::strand>{
            strand_
        };
    }

    asio::executor_work_guard<asio::io_context::executor_type> work_guard()
    {
        return asio::executor_work_guard<asio::io_context::executor_type>{
            strand_.context().get_executor()};
    }

    lua_State* L()
    {
        return L_;
    }

    void close();

    lua_State* current_fiber()
    {
        return current_fiber_;
    }

    bool valid()
    {
        return valid_;
    }

    void fiber_resume(lua_State* fiber)
    {
        fiber_prologue(fiber);
        reclaim_reserved_zone();
        int res = lua_resume(fiber, 0);
        fiber_epilogue(res);
    }

    void fiber_resume_trivial(lua_State* fiber)
    {
        fiber_prologue_trivial(fiber);
        int res = lua_resume(fiber, 0);
        fiber_epilogue(res);
    }

    void fiber_prologue_trivial(lua_State* new_current_fiber);
    void fiber_epilogue(int resume_result);

    void fiber_prologue(lua_State* new_current_fiber)
    {
        fiber_prologue_trivial(new_current_fiber);
        enable_reserved_zone();

        lua_pushnil(current_fiber_);
        set_interrupter(current_fiber_);
    }

    void notify_errmem();
    void enable_reserved_zone();
    void reclaim_reserved_zone();

    void notify_deadlock(std::string msg);
    void notify_cleanup_error(lua_State* coro);

    inbox_t inbox;

    boost::intrusive::list<
        pending_operation,
        boost::intrusive::constant_time_size<false>
    > pending_operations;

private:
    boost::asio::io_context::strand strand_;
    bool valid_;
    bool lua_errmem;
    bool suppress_tail_errors = false;
    lua_State* L_;
    lua_State* current_fiber_;
    std::vector<std::string> deadlock_errors;
    void* failed_cleanup_handler_coro = nullptr;
};

vm_context& get_vm_context(lua_State* L);

void push(lua_State* L, const std::error_code& ec);

inline void push(lua_State* L, std::errc ec)
{
    return push(L, make_error_code(ec));
}

// gets value from top of the stack
std::variant<std::string_view, std::error_code> inspect_errobj(lua_State* L);

std::string errobj_to_string(std::variant<std::string_view, std::error_code> o);

inline void push(lua_State* L, std::string_view str)
{
    lua_pushlstring(L, str.data(), str.size());
}

inline void push(lua_State* L, const std::string& str)
{
    push(L, std::string_view{str});
}

inline void push(lua_State* L, const std::filesystem::path& path)
{
    auto p = path.string();
    lua_pushlstring(L, p.data(), p.size());
}

inline std::string_view tostringview(lua_State* L, int index = -1)
{
    std::size_t len;
    const char* buf = lua_tolstring(L, index, &len);
    return std::string_view{buf, len};
}

inline void rawgetp(lua_State* L, int pseudoindex, const void* p)
{
    lua_pushlightuserdata(L, const_cast<void*>(p));
    lua_rawget(L, pseudoindex);
}

template<class T>
inline void finalize(lua_State* L, int index = 1)
{
    auto obj = reinterpret_cast<T*>(lua_touserdata(L, index));
    assert(obj);
    obj->~T();
}

template<class T>
inline int finalizer(lua_State* L)
{
    finalize<T>(L);
    return 0;
}

enum class lua_errc
{
    file = LUA_ERRFILE,
    syntax = LUA_ERRSYNTAX,
    run = LUA_ERRRUN,
    err = LUA_ERRERR,
    mem = LUA_ERRMEM,
};

const std::error_category& lua_category();

inline std::error_code make_error_code(lua_errc e)
{
    return std::error_code{static_cast<int>(e), lua_category()};
}

class lua_exception: public std::system_error
{
public:
    lua_exception(int ev);
    lua_exception(int ev, const std::string& what_arg);
    lua_exception(int ev, const char* what_arg);
    lua_exception(lua_errc ec);
    lua_exception(lua_errc ec, const std::string& what_arg);
    lua_exception(lua_errc ec, const char* what_arg);
    lua_exception(const lua_exception&) noexcept = default;

    lua_exception& operator=(const lua_exception&) noexcept = default;
};

enum class errc {
    invalid_module_name = 1,
    module_not_found,
    root_cannot_import_parent,
    cyclic_import,
    leaf_cannot_import_child,
    failed_to_load_module,
    only_main_fiber_may_import,
    bad_root_context,
    bad_index,
    bad_coroutine,
    suspension_already_allowed,
    interruption_already_allowed,
    forbid_suspend_block,
    interrupted,
    unmatched_scope_cleanup,
    channel_closed,
    no_senders,
};

const std::error_category& category();

inline std::error_code make_error_code(errc e)
{
    return std::error_code{static_cast<int>(e), category()};
}

class exception: public std::system_error
{
public:
    exception(int ev);
    exception(int ev, const std::string& what_arg);
    exception(int ev, const char* what_arg);
    exception(errc ec);
    exception(errc ec, const std::string& what_arg);
    exception(errc ec, const char* what_arg);
    exception(const exception&) noexcept = default;

    exception& operator=(const exception&) noexcept = default;
};

namespace detail {
bool unsafe_can_suspend(vm_context& vm_ctx, lua_State* L);
bool unsafe_can_suspend2(vm_context& vm_ctx, lua_State* L);
} // namespace detail

} // namespace emilua

template<>
struct std::is_error_code_enum<emilua::lua_errc>: std::true_type {};

template<>
struct std::is_error_code_enum<emilua::errc>: std::true_type {};

namespace emilua {

inline actor_address::actor_address(vm_context& vm_ctx)
    : dest{vm_ctx.weak_from_this()}
    , work_guard{vm_ctx.work_guard()}
{
    ++vm_ctx.inbox.nsenders;
}

inline actor_address::actor_address(const actor_address& o)
    : dest{o.dest}
    , work_guard{o.work_guard}
{
    auto vm_ctx = dest.lock();
    if (!vm_ctx)
        return;

    ++vm_ctx->inbox.nsenders;
}

inline actor_address& actor_address::operator=(actor_address&& o)
{
    dest = std::move(o.dest);
    work_guard.~executor_work_guard();
    new (&work_guard) asio::executor_work_guard<
        asio::io_context::executor_type>{std::move(o.work_guard)};
    return *this;
}

inline actor_address::~actor_address()
{
    auto vm_ctx = dest.lock();
    if (!vm_ctx)
        return;

    if (--vm_ctx->inbox.nsenders != 0)
        return;

    vm_ctx->strand().post([vm_ctx]() {
        if (vm_ctx->inbox.nsenders.load() != 0) {
            // another fiber from the actor already created a new sender
            return;
        }

        auto recv_fiber = vm_ctx->inbox.recv_fiber;
        if (recv_fiber == nullptr)
            return;

        vm_ctx->inbox.recv_fiber = nullptr;
        vm_ctx->inbox.work_guard.reset();

        vm_ctx->fiber_prologue(recv_fiber);
        push(recv_fiber, errc::no_senders);
        vm_ctx->reclaim_reserved_zone();
        int res = lua_resume(recv_fiber, 1);
        vm_ctx->fiber_epilogue(res);
    }, std::allocator<void>{});
}

inline inbox_t::sender_state::sender_state(vm_context& vm_ctx)
    : vm_ctx(vm_ctx.shared_from_this())
    , work_guard(vm_ctx.work_guard())
    , fiber(vm_ctx.current_fiber())
    , msg{std::in_place_type<bool>, false}
{}

inline inbox_t::sender_state::sender_state(vm_context& vm_ctx, lua_State* fiber)
    : vm_ctx(vm_ctx.shared_from_this())
    , work_guard(vm_ctx.work_guard())
    , fiber(fiber)
    , msg{std::in_place_type<bool>, false}
{}

inline inbox_t::sender_state::sender_state(sender_state&& o)
    : vm_ctx(std::move(o.vm_ctx))
    , work_guard(std::move(o.work_guard))
    , fiber(o.fiber)
    , msg(std::move(o.msg))
    , wake_on_destruct(o.wake_on_destruct)
{
    o.wake_on_destruct = false;
}

inline inbox_t::sender_state::~sender_state()
{
    if (!wake_on_destruct)
        return;

    vm_ctx->strand().post([vm_ctx=vm_ctx, fiber=fiber]() {
        vm_ctx->fiber_prologue(fiber);
        push(fiber, errc::channel_closed);
        vm_ctx->reclaim_reserved_zone();
        int res = lua_resume(fiber, 1);
        vm_ctx->fiber_epilogue(res);
    }, std::allocator<void>{});
}

inline
inbox_t::sender_state&
inbox_t::sender_state::operator=(inbox_t::sender_state&& o)
{
    if (wake_on_destruct) {
        vm_ctx->strand().post([vm_ctx=vm_ctx, fiber=fiber]() {
            vm_ctx->fiber_prologue(fiber);
            push(fiber, errc::channel_closed);
            vm_ctx->reclaim_reserved_zone();
            int res = lua_resume(fiber, 1);
            vm_ctx->fiber_epilogue(res);
        }, std::allocator<void>{});
    }

    vm_ctx = std::move(o.vm_ctx);
    work_guard.~executor_work_guard();
    new (&work_guard) asio::executor_work_guard<
        asio::io_context::executor_type>{std::move(o.work_guard)};
    fiber = o.fiber;
    msg = std::move(o.msg);
    wake_on_destruct = o.wake_on_destruct;

    o.wake_on_destruct = false;
    return *this;
}

} // namespace emilua

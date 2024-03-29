/* Copyright (c) 2020, 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#include <charconv>
#include <locale>
#include <new>

#include <fmt/ostream.h>
#include <fmt/format.h>

#include <boost/nowide/iostream.hpp>
#include <boost/hana/maximum.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/plus.hpp>

#include <emilua/detail/core.hpp>
#include <emilua/fiber.hpp>
#include <emilua/actor.hpp>

namespace emilua {

bool stdout_has_color;
char raw_unpack_key;
char raw_xpcall_key;
char raw_pcall_key;
char raw_error_key;
char raw_type_key;
char raw_pairs_key;
char raw_ipairs_key;
char raw_next_key;
char raw_setmetatable_key;
char raw_getmetatable_key;

#if BOOST_OS_LINUX
void* clone_stack_address;
#endif // BOOST_OS_LINUX

#if BOOST_OS_UNIX
char*** app_context::environp;
#endif // BOOST_OS_UNIX

asio::io_context::id properties_service::id;

std::string_view log_domain<default_log_domain>::name = "emilua";
int log_domain<default_log_domain>::log_level = /*LOG_WARNING=*/4;

namespace detail {
char context_key;
char error_code_mt_key;
char error_category_mt_key;
char rdf_error_category_module_mt_key;
} // namespace detail

const char* rdf_error_category::name() const noexcept
{
    return name_.c_str();
}

std::string rdf_error_category::message(int value) const noexcept
{
    std::string curloc{std::locale{}.name()};
    std::string_view tgt{"LC_MESSAGES="};
    if (auto idx = curloc.find(tgt) ; idx != std::string::npos) {
        curloc.erase(0, idx + tgt.size());
    }
    if (auto it = messages.find(value) ; it != messages.end()) {
        // TODO: perform a binary search first to find the lower/upper bounds,
        // and thus reduce the iteration range
        for (const auto& m: it->second) {
            // TODO: what to do about matches that are more specific (e.g. "en"
            // vs "en_US")?
            if (!m.first.empty() && curloc.starts_with(m.first))
                return m.second;
        }
        return it->second.at(std::string{});
    } else {
        return "Unknown";
    }
}

std::error_condition rdf_error_category::default_error_condition(int code) const
    noexcept
{
    // TODO: RDF-defined error categories should be able to map error conditions
    // for other categories too. A draft follows:
    //
    // @prefix cat: <https://schema.emilua.org/error_category/1/#>.
    // <about:emilua-module>
    //     a <https://schema.emilua.org/error_category/1/>;
    //     cat:error [
    //         cat:code 1;
    //         cat:alias "ED";
    //         cat:message "?";
    //         cat:generic_error [
    //             cat:code 2; # or the string-alias
    //
    //             # same syntax from module import, but we deliberately
    //             # avoid using <https://schema.emilua.org/module/0/#import>
    //             # or the likes to ensure the error_category schema remains
    //             # more stable/frozen/independent over time
    //             cat:foreign_category "path/to/error/category"
    //         ]
    //     ].
    //
    // An use-case for this future feature:
    // <http://breese.github.io/2017/05/12/customizing-error-codes.html>.

    if (auto it = generic_errors.find(code) ; it != generic_errors.end()) {
        return {it->second, std::generic_category()};
    } else {
        return std::error_category::default_error_condition(code);
    }
}

void app_context::init_log_domain(std::string_view name, int& log_level)
{
    auto it = app_env.find("EMILUA_LOG_LEVELS");
    if (it == app_env.end())
        return;

    std::string_view env = it->second;
    auto pos = env.find(name);
    if (pos == std::string_view::npos)
        return;

    auto range = env.substr(pos);
    range.remove_prefix(name.size() + /*the ':' char*/1);
    int level;
    auto res = std::from_chars(
        range.data(), range.data() + range.size(), level);
    if (res.ec != std::errc{})
        return;

    log_level = level;
}

void app_context::vlog(int priority, std::string_view domain,
                       fmt::string_view format_str, fmt::format_args args)
{
    thread_local fmt::memory_buffer buf;
    buf.clear();
    auto out = fmt::appender(buf);
    switch (priority) {
    case /*LOG_EMERG=*/0:
    case /*LOG_ALERT=*/1:
    case /*LOG_CRIT=*/2:
    case /*LOG_ERR=*/3:
        if (stdout_has_color) {
            fmt::format_to(out, FMT_STRING("<_>\033[31;1m[{}] "), domain);
            buf.data()[1] = '0' + priority;
            break;
        }
        [[fallthrough]];
    case /*LOG_WARNING=*/4:
        if (stdout_has_color) {
            fmt::format_to(out, FMT_STRING("<4>\033[93;1m[{}] "), domain);
            break;
        }
        [[fallthrough]];
    default:
        fmt::format_to(out, FMT_STRING("<_>[{}] "), domain);
        buf.data()[1] = '0' + priority;
    }
    fmt::vformat_to(out, format_str, args);
    if (stdout_has_color && priority <= /*LOG_WARNING=*/4) {
        std::string_view tail = "\033[22;39m\n";
        buf.append(tail.data(), tail.data() + tail.size());
    } else {
        buf.resize(buf.size() + 1);
        buf.data()[buf.size() - 1] = '\n';
    }
    try {
        nowide::cerr.write(buf.data(), buf.size());
    } catch (const std::ios_base::failure&) {}
}

properties_service::properties_service(asio::execution_context& ctx,
                                       int concurrency_hint)
    : asio::execution_context::service(ctx)
    , concurrency_hint(concurrency_hint)
{}

properties_service::properties_service(asio::execution_context& ctx)
    : asio::execution_context::service(ctx)
    , concurrency_hint(-1)
{
    throw std::logic_error("properties_service not initialized");
}

void properties_service::shutdown()
{}

vm_context::vm_context(emilua::app_context& appctx, strand_type strand)
    : appctx(appctx)
    , strand_(std::move(strand))
    , valid_(true)
    , lua_errmem(false)
    , exit_request(false)
    , L_(luaL_newstate())
    , current_fiber_(nullptr)
{
    if (!L_)
        throw std::bad_alloc{};
}

vm_context::~vm_context()
{
    if (valid_)
        close();
}

void vm_context::close()
{
    if (!valid_)
        return;

    if (lua_errmem) {
        constexpr auto spec{FMT_STRING(
            "{}VM {:p} forcibly closed due to '{}LUA_ERRMEM{}'{}\n"
        )};

        std::string_view red{"\033[31;1m"};
        std::string_view underline{"\033[4m"};
        std::string_view reset_red{"\033[22;39m"};
        std::string_view reset_underline{"\033[24m"};
        if (!stdout_has_color)
            red = underline = reset_red = reset_underline = {};

        if (/*LOG_ERR=*/3 <= log_domain<default_log_domain>::log_level) {
            try {
                fmt::print(
                    nowide::cerr,
                    spec,
                    red,
                    static_cast<const void*>(L_),
                    underline, reset_underline,
                    reset_red);
            } catch (const std::ios_base::failure&) {}
        }

        suppress_tail_errors = true;
    }

    lua_close(L_);

    if (!suppress_tail_errors && failed_cleanup_handler_coro) {
        std::string_view red{"\033[31;1m"};
        std::string_view reset_red{"\033[22;39m"};
        if (!stdout_has_color)
            red = reset_red = {};

        constexpr auto spec{FMT_STRING(
            "{}VM {:p} forcibly closed due to error raised"
            " on cleanup handler from coroutine {:p}{}\n"
        )};
        std::string errors;
        for (auto& e: deadlock_errors) {
            errors.push_back('\t');
            errors += e;
            errors.push_back('\n');
        }
        if (/*LOG_ERR=*/3 <= log_domain<default_log_domain>::log_level) {
            try {
                fmt::print(
                    nowide::cerr,
                    spec,
                    red, static_cast<void*>(L_), failed_cleanup_handler_coro,
                    reset_red);
            } catch (const std::ios_base::failure&) {}
        }
        suppress_tail_errors = true;
    }

    if (!suppress_tail_errors && deadlock_errors.size() != 0) {
        std::string_view red{"\033[31;1m"};
        std::string_view dim{"\033[2m"};
        std::string_view reset_red{"\033[22;39m"};
        std::string_view reset_dim{"\033[22m"};
        if (!stdout_has_color)
            red = dim = reset_red = reset_dim = {};

        constexpr auto spec{FMT_STRING(
            "{}Possible deadlock(s) detected during VM {:p} shutdown{}:\n"
            "{}{}{}"
        )};
        std::string errors;
        for (auto& e: deadlock_errors) {
            errors.push_back('\t');
            errors += e;
            errors.push_back('\n');
        }
        if (/*LOG_ERR=*/3 <= log_domain<default_log_domain>::log_level) {
            try {
                fmt::print(
                    nowide::cerr,
                    spec,
                    red, static_cast<void*>(L_), reset_red,
                    dim, errors, reset_dim);
            } catch (const std::ios_base::failure&) {}
        }
    }

    valid_ = false;
    L_ = nullptr;
    inbox.recv_fiber = nullptr;
    inbox.open = false;
    inbox.work_guard.reset();

    for (auto& m: inbox.incoming) {
        m.wake_on_destruct = true;
    }
    inbox.incoming.clear();

    pending_operations.clear_and_dispose([](pending_operation* op) {
        op->cancel();

        if (!op->shared_ownership) {
            // `vm_context` destructor calls `close()`, but only if `close()`
            // hasn't been called before. This means that if extra operations
            // were appended, they won't be deleted here.
            //
            // However there is no leak. We don't need a different
            // behaviour. The pattern under which pending operations fit is fine
            // for this implementation.
            delete op;
        }
    });
}

void vm_context::fiber_epilogue(int resume_result)
{
    assert(valid_);
    assert(current_fiber_ != async_event_thread_);
    if (lua_errmem || failed_cleanup_handler_coro || exit_request) {
        close();
        return;
    }
    if (!lua_checkstack(current_fiber_, LUA_MINSTACK)) {
        lua_errmem = true;
        close();
        return;
    }
    switch (resume_result) {
    case LUA_YIELD:
        // Nothing to do. Fiber is awakened by the completion handler.
        break;
    case 0: //< finished without errors
    case LUA_ERRRUN: { //< a runtime error
#ifndef NDEBUG
        if (resume_result == LUA_ERRRUN &&
            lua_type(current_fiber_, -1) == LUA_TSTRING) {
            assert(tostringview(current_fiber_, -1) !=
                   "cannot resume non-suspended coroutine");
        }
#endif // NDEBUG

        rawgetp(current_fiber_, LUA_REGISTRYINDEX, &fiber_list_key);

        lua_pushthread(current_fiber_);
        lua_rawget(current_fiber_, -2);
        lua_rawgeti(current_fiber_, -1, FiberDataIndex::JOINER);
        int joiner_type = lua_type(current_fiber_, -1);
        if (joiner_type == LUA_TBOOLEAN) {
            // Detached
            assert(lua_toboolean(current_fiber_, -1) == 0);
            bool is_main = L() == current_fiber_;

            if (is_main) {
                lua_pushlightuserdata(current_fiber_, &inbox_key);
                lua_pushnil(current_fiber_);
                lua_rawset(current_fiber_, LUA_REGISTRYINDEX);

                if (!inbox.imported) {
                    inbox.open = false;
                    for (auto& m: inbox.incoming) {
                        m.wake_on_destruct = true;
                    }
                    inbox.incoming.clear();
                }
            }

            if (resume_result == LUA_ERRRUN) {
                try {
                    lua_rawgeti(current_fiber_, -2, FiberDataIndex::STACKTRACE);
                    lua_pushvalue(current_fiber_, -5);
                    auto err_obj = inspect_errobj(current_fiber_);
                    if (auto e = std::get_if<std::error_code>(&err_obj) ;
                        !e || *e != errc::interrupted || is_main) {
                        print_panic(current_fiber_, is_main,
                                    errobj_to_string(err_obj),
                                    tostringview(current_fiber_, -2));
                    }
                    if (is_main) {
                        // TODO: call uncaught-hook
                        suppress_tail_errors = true;
                        close();
                        return;
                    }
                    lua_pop(current_fiber_, 2);
                } catch (...) {
                    lua_errmem = true;
                    close();
                    return;
                }
            }

            lua_pushthread(current_fiber_);
            lua_pushnil(current_fiber_);
            lua_rawset(current_fiber_, -5);
            // TODO (?): force a full GC round on `L()` now
        } else if (joiner_type == LUA_TTHREAD) {
            // Joined
            lua_State* joiner = lua_tothread(current_fiber_, -1);

            lua_pushinteger(
                current_fiber_,
                (resume_result == 0)
                ? FiberStatus::FINISHED_SUCCESSFULLY
                : FiberStatus::FINISHED_WITH_ERROR);
            lua_rawseti(current_fiber_, -3, FiberDataIndex::STATUS);

            lua_rawgeti(current_fiber_, -2, FiberDataIndex::USER_HANDLE);
            auto join_handle = static_cast<fiber_handle*>(
                lua_touserdata(current_fiber_, -1));

            lua_pop(current_fiber_, 4);

            int nret = (resume_result == 0) ? lua_gettop(current_fiber_) : 1;
            if (!lua_checkstack(joiner, nret + 1 + LUA_MINSTACK)) {
                lua_errmem = true;
                close();
                return;
            }
            lua_pushboolean(joiner, resume_result == 0);
            lua_xmove(current_fiber_, joiner, nret);

            bool interruption_caught = false;
            if (resume_result == LUA_ERRRUN) {
                try {
                    auto err_obj = inspect_errobj(joiner);
                    if (auto e = std::get_if<std::error_code>(&err_obj) ;
                        e && *e == errc::interrupted) {
                        lua_pop(joiner, 2);
                        lua_pushboolean(joiner, 1);
                        nret = 0;
                        interruption_caught = true;
                    }
                } catch (...) {
                    lua_errmem = true;
                    close();
                    return;
                }
            }
            if (join_handle) {
                join_handle->fiber = nullptr;
                join_handle->interruption_caught = interruption_caught;
            } else {
                // do nothing; this branch executes only for modules' fibers
                // and modules' fibers never expose a join handle to the user
            }

            current_fiber_ = joiner;
            lua_pushnil(joiner);
            set_interrupter(joiner, *this);
            int res = lua_resume(joiner, nret + 1);
            // I'm assuming the compiler will eliminate this tail recursive call
            // or else we may experience stack overflow on really really long
            // join()-chains. Still better than the round-trip of post
            // semantics.
#ifdef __has_attribute
#  if __has_attribute(musttail)
            __attribute__((musttail))
#  endif // __has_attribute(musttail)
#endif // defined(__has_attribute)
            return fiber_epilogue(res);
        } else {
            // Handle still alive
            lua_pushinteger(
                current_fiber_,
                (resume_result == 0)
                ? FiberStatus::FINISHED_SUCCESSFULLY
                : FiberStatus::FINISHED_WITH_ERROR);
            lua_rawseti(current_fiber_, -3, FiberDataIndex::STATUS);
            lua_pop(current_fiber_, 3);
        }
        break;
    }
    case LUA_ERRMEM: //< memory allocation error
        lua_errmem = true;
        close();
        break;
    case LUA_ERRERR:
    default:
        assert(false);
    }
}

void vm_context::notify_errmem()
{
    lua_errmem = true;
}

void vm_context::notify_exit_request()
{
    exit_request = true;
}

void vm_context::notify_deadlock(std::string msg)
{
    deadlock_errors.emplace_back(std::move(msg));
}

void vm_context::notify_cleanup_error(lua_State* coro)
{
    failed_cleanup_handler_coro = coro;
}

vm_context& get_vm_context(lua_State* L)
{
    rawgetp(L, LUA_REGISTRYINDEX, &detail::context_key);
    auto ret = static_cast<vm_context*>(lua_touserdata(L, -1));
    assert(ret);
    lua_pop(L, 1);
    return *ret;
}

void push(lua_State* L, const std::error_code& ec)
{
    if (!ec) {
        lua_pushnil(L);
        return;
    }

    lua_createtable(L, /*narr=*/0, /*nrec=*/2);

    lua_pushliteral(L, "code");
    lua_pushinteger(L, ec.value());
    lua_rawset(L, -3);

    lua_pushliteral(L, "category");
    {
        *static_cast<const std::error_category**>(
            lua_newuserdata(L, sizeof(void*))) = &ec.category();
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_mt_key);
        setmetatable(L, -2);
    }
    lua_rawset(L, -3);

    rawgetp(L, LUA_REGISTRYINDEX, &detail::error_code_mt_key);
    setmetatable(L, -2);
}

std::variant<std::string_view, std::error_code> inspect_errobj(lua_State* L)
{
    std::variant<std::string_view, std::error_code> ret = std::string_view{};

    switch (lua_type(L, -1)) {
    case LUA_TSTRING:
        ret = tostringview(L, -1);
        break;
    case LUA_TTABLE: {
        int ev;
        std::error_category* cat;
        lua_pushliteral(L, "code");
        lua_rawget(L, -2);
        if (lua_type(L, -1) != LUA_TNUMBER) {
            lua_pop(L, 1);
            break;
        }
        ev = lua_tointeger(L, -1);
        lua_pushliteral(L, "category");
        lua_rawget(L, -3);
        if (lua_type(L, -1) != LUA_TUSERDATA || !lua_getmetatable(L, -1)) {
            lua_pop(L, 2);
            break;
        }
        rawgetp(L, LUA_REGISTRYINDEX, &detail::error_category_mt_key);
        if (!lua_rawequal(L, -1, -2)) {
            lua_pop(L, 4);
            break;
        }
        cat = *static_cast<std::error_category**>(lua_touserdata(L, -3));
        ret = std::error_code{ev, *cat};
        lua_pop(L, 4);
        break;
    }
    }

    return ret;
}

std::string errobj_to_string(std::variant<std::string_view, std::error_code> o)
{
    return std::visit(
        [](auto&& arg) -> std::string {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::string_view>)
                return std::string{arg};
            else if constexpr (std::is_same_v<T, std::error_code>)
                return arg.category().message(arg.value());
        },
        o
    );
}

void set_interrupter(lua_State* L, vm_context& vm_ctx)
{
    auto current_fiber = vm_ctx.current_fiber();
    bool interruption_disabled;

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::INTERRUPTION_DISABLED);
    switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
        interruption_disabled = lua_toboolean(L, -1);
        break;
    case LUA_TNUMBER:
        interruption_disabled = lua_tointeger(L, -1) > 0;
        break;
    default: assert(false);
    }
    if (interruption_disabled) {
        lua_pop(L, 4);
        return;
    }

    lua_pushvalue(L, -4);
    lua_rawseti(L, -3, FiberDataIndex::INTERRUPTER);
    lua_pop(L, 4);
}

asio::cancellation_slot
set_default_interrupter(lua_State* L, vm_context& vm_ctx)
{
    auto current_fiber = vm_ctx.current_fiber();
    bool interruption_disabled;

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::INTERRUPTION_DISABLED);
    switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
        interruption_disabled = lua_toboolean(L, -1);
        break;
    case LUA_TNUMBER:
        interruption_disabled = lua_tointeger(L, -1) > 0;
        break;
    default: assert(false);
    }
    if (interruption_disabled) {
        lua_pop(L, 3);
        return {};
    }

    lua_rawgeti(L, -2, FiberDataIndex::ASIO_CANCELLATION_SIGNAL);
    auto cancel_signal = static_cast<asio::cancellation_signal*>(
        lua_touserdata(L, -1));
    lua_rawgeti(L, -3, FiberDataIndex::DEFAULT_EMIT_SIGNAL_INTERRUPTER);
    lua_rawseti(L, -4, FiberDataIndex::INTERRUPTER);
    lua_pop(L, 4);

    if (!cancel_signal)
        return {};

    return cancel_signal->slot();
}

int throw_enosys(lua_State* L)
{
    push(L, std::errc::function_not_supported);
    return lua_error(L);
}

class lua_category_impl: public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
};

const char* lua_category_impl::name() const noexcept
{
    return "lua";
}

std::string lua_category_impl::message(int value) const noexcept
{
    switch (value) {
    case static_cast<int>(lua_errc::file):
        return "cannot open/read the file";
    case static_cast<int>(lua_errc::syntax):
        return "syntax error during pre-compilation";
    case static_cast<int>(lua_errc::run):
        return "runtime error";
    case static_cast<int>(lua_errc::err):
        return "error while running the error handler function";
    case static_cast<int>(lua_errc::mem):
        return "memory allocation error";
    default:
        return {};
    }
}

const std::error_category& lua_category()
{
    static lua_category_impl cat;
    return cat;
}

lua_exception::lua_exception(int ev)
    : std::system_error{ev, lua_category()}
{}

lua_exception::lua_exception(int ev, const std::string& what_arg)
    : std::system_error{ev, lua_category(), what_arg}
{}

lua_exception::lua_exception(int ev, const char* what_arg)
    : std::system_error{ev, lua_category(), what_arg}
{}

lua_exception::lua_exception(lua_errc ec)
    : std::system_error{ec}
{}

lua_exception::lua_exception(lua_errc ec, const std::string& what_arg)
    : std::system_error{ec, what_arg}
{}

lua_exception::lua_exception(lua_errc ec, const char* what_arg)
    : std::system_error{ec, what_arg}
{}

class category_impl: public std::error_category
{
public:
    const char* name() const noexcept override;
    std::string message(int value) const noexcept override;
};

const char* category_impl::name() const noexcept
{
    return "emilua.core";
}

std::string category_impl::message(int value) const noexcept
{
    switch (value) {
    case static_cast<int>(errc::invalid_module_name):
        return "Cannot have a module with this name";
    case static_cast<int>(errc::module_not_found):
        return "Module not found";
    case static_cast<int>(errc::root_cannot_import_parent):
        return "The root module doesn't have a parent and can't reference one";
    case static_cast<int>(errc::cyclic_import):
        return "The module you're trying to import has a dependency on the"
            " current module (and it is partially loaded already)";
    case static_cast<int>(errc::leaf_cannot_import_child):
        return "A leaf module cannot import child modules";
    case static_cast<int>(errc::only_main_fiber_may_import):
        return "You can only import modules from the main fiber";
    case static_cast<int>(errc::bad_root_context):
        return "Bad root context";
    case static_cast<int>(errc::bad_index):
        return "Requested key wasn't found in the table/userdata";
    case static_cast<int>(errc::bad_coroutine):
        return "The fiber coroutine is reserved to the scheduler";
    case static_cast<int>(errc::suspension_already_allowed):
        return "Suspension already allowed";
    case static_cast<int>(errc::interruption_already_allowed):
        return "Interrupt-ability already allowed";
    case static_cast<int>(errc::forbid_suspend_block):
        return "EPERM within a forbid-suspend block";
    case static_cast<int>(errc::interrupted):
        return "Fiber interrupted";
    case static_cast<int>(errc::unmatched_scope_cleanup):
        return "scope_cleanup_pop() called w/o a matching scope_cleanup_push()";
    case static_cast<int>(errc::channel_closed):
        return "Channel closed";
    case static_cast<int>(errc::no_senders):
        return "Broadcast the address before attempting to receive on it";
    case static_cast<int>(errc::internal_module):
        return "Lua code cannot import this module directly";
    case static_cast<int>(errc::raise_error):
        return "std::raise() failed";
    case static_cast<int>(errc::bad_rdf):
        return "Failed to parse RDF";
    case static_cast<int>(errc::bad_rdf_module):
        return "Parsed RDF doesn't contain a recognized Emilua module";
    case static_cast<int>(errc::bad_rdf_error_category):
        return "Parsed RDF module contains an invalid error category";
    case static_cast<int>(errc::broken_promise):
        return "Broken promise";
    case static_cast<int>(errc::promise_already_satisfied):
        return "Promise already satisfied";
    case static_cast<int>(errc::current_module_not_known):
        return "Global _FILE is missing or invalid";
    default:
        return {};
    }
}

const std::error_category& category()
{
    static category_impl cat;
    return cat;
}

exception::exception(int ev)
    : std::system_error{ev, category()}
{}

exception::exception(int ev, const std::string& what_arg)
    : std::system_error{ev, category(), what_arg}
{}

exception::exception(int ev, const char* what_arg)
    : std::system_error{ev, category(), what_arg}
{}

exception::exception(errc ec)
    : std::system_error{ec}
{}

exception::exception(errc ec, const std::string& what_arg)
    : std::system_error{ec, what_arg}
{}

exception::exception(errc ec, const char* what_arg)
    : std::system_error{ec, what_arg}
{}

bool detail::unsafe_can_suspend(vm_context& vm_ctx, lua_State* L)
{
    auto current_fiber = vm_ctx.current_fiber();
    if (current_fiber == vm_ctx.async_event_thread_) {
        lua_pushliteral(current_fiber, "attempt to suspend a system fiber");
        return false;
    }

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::SUSPENSION_DISALLOWED);
    if (lua_tointeger(L, -1) != 0) {
        push(L, emilua::errc::forbid_suspend_block);
        return false;
    }
    lua_rawgeti(L, -2, FiberDataIndex::INTERRUPTION_DISABLED);
    bool interruption_disabled;
    switch (lua_type(L, -1)) {
    case LUA_TBOOLEAN:
        interruption_disabled = lua_toboolean(L, -1);
        break;
    case LUA_TNUMBER:
        interruption_disabled = lua_tointeger(L, -1) > 0;
        break;
    default: assert(false);
    }
    if (interruption_disabled) {
        lua_pop(L, 4);
        return true;
    }
    lua_rawgeti(L, -3, FiberDataIndex::INTERRUPTED);
    if (lua_toboolean(L, -1) == 1) {
        push(L, emilua::errc::interrupted);
        return false;
    }
    lua_pop(L, 5);
    return true;
}

bool detail::unsafe_can_suspend2(vm_context& vm_ctx, lua_State* L)
{
    auto current_fiber = vm_ctx.current_fiber();
    if (current_fiber == vm_ctx.async_event_thread_) {
        lua_pushliteral(current_fiber, "attempt to suspend a system fiber");
        return false;
    }

    rawgetp(L, LUA_REGISTRYINDEX, &fiber_list_key);
    lua_pushthread(current_fiber);
    lua_xmove(current_fiber, L, 1);
    lua_rawget(L, -2);
    lua_rawgeti(L, -1, FiberDataIndex::SUSPENSION_DISALLOWED);
    if (lua_tointeger(L, -1) != 0) {
        push(L, emilua::errc::forbid_suspend_block);
        return false;
    }
    lua_pop(L, 3);
    return true;
}

} // namespace emilua

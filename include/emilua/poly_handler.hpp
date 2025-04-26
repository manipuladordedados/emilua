// Copyright (c) 2018, 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/config.h>

#include <type_traits>
#include <functional>
#include <optional>
#include <utility>
#include <memory>

#include <memory_resource>

#include <boost/core/ignore_unused.hpp>

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/associated_allocator.hpp>
#include <asio/associated_executor.hpp>
#include <asio/executor.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/executor.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua {

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = ::asio;
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

template<class>
struct poly_handler; /* undefined */

namespace detail {

// associated allocators helpers {{{

// placeholder to use as fallback allocator in allocator traits
struct not_an_allocator_t {};

template<class T>
struct is_poly_alloc: std::false_type {};

template<class T>
struct is_poly_alloc<
    std::pmr::polymorphic_allocator<T>
>: std::true_type {};

// }}}

// TODO: add support to immediate executors
template<class R, class... Args>
class poly_handler_interface
{
public:
    virtual ~poly_handler_interface() = default;

    virtual poly_handler_interface<R, Args...>* clone() const = 0;

    virtual R call(Args... args) = 0;

    virtual std::pmr::memory_resource* resource() const = 0;
    virtual asio::executor get_executor() const = 0;
};

template<class F, class R, class... Args>
class poly_handler_impl: public poly_handler_interface<R, Args...>
{
public:
    poly_handler_impl(F f)
        : wrapped(std::move(f))
    {
        using asio::associated_allocator_t;
        if constexpr (is_poly_alloc<associated_allocator_t<F>>::value) {
            alloc.emplace(asio::get_associated_allocator(f));
        } else {
            using A = asio::associated_allocator_t<
                F, detail::not_an_allocator_t
            >;
            static_assert(std::is_same_v<A, detail::not_an_allocator_t>,
                          "Only supported associated allocator type for type"
                          " erasure is std::pmr::polymorphic_allocator<T>");
        }
    }

    poly_handler_impl(const poly_handler_impl<F, R, Args...> &o)
        : wrapped(o.wrapped)
    {}

    poly_handler_interface<R, Args...>* clone() const override
    {
        return new poly_handler_impl<F, R, Args...>(*this);
    }

    R call(Args... args) override
    {
        return wrapped(args...);
    }

    std::pmr::memory_resource* resource() const override
    {
        if (alloc)
            return alloc->resource();
        else
            return NULL;
    }

    asio::executor get_executor() const override
    {
        asio::executor fallback_executor;
        return asio::get_associated_executor(wrapped, fallback_executor);
    }

private:
    F wrapped;
    std::optional<std::pmr::polymorphic_allocator<void>> alloc;
};

template<class R, class... Args>
detail::poly_handler_interface<R, Args...>*
get_impl(const poly_handler<R(Args...)> &h);

} // namespace detail

// Completion handler wrapper that will preserve associated allocators and
// associated executors (and more if ASIO API expands to have more associated
// characteristics).
template<class R, class... Args>
struct poly_handler<R(Args...)>
{
public:
    using result_type = R;

    poly_handler() {}

    template<class F>
    poly_handler(F f)
        : impl(new detail::poly_handler_impl<F, R, Args...>{f})
    {}

    poly_handler(poly_handler<R(Args...)> &&o)
        : impl(o.impl)
    {
        o.impl = NULL;
    }

    poly_handler(const poly_handler<R(Args...)> &o)
        : impl(o.impl ? o.impl->clone() : NULL)
    {}

    template<class F>
    poly_handler<R(Args...)>& operator=(F f)
    {
        delete impl;
        impl = NULL;
        impl = new detail::poly_handler_impl<F, R, Args...>{f};
        return *this;
    }

    poly_handler<R(Args...)>&
    operator=(poly_handler<R(Args...)> &&o)
    {
        delete impl;
        impl = o.impl;
        o.impl = NULL;
        return *this;
    }

    ~poly_handler()
    {
        delete impl;
    }

    R operator()(Args... args) const
    {
        if (!impl)
            throw std::bad_function_call();

        return impl->call(args...);
    }

    explicit operator bool() const
    {
        return impl;
    }

    std::pmr::memory_resource* associated_resource() const
    {
        if (!impl)
            return NULL;

        return impl->resource();
    }

    template<class R2, class... Args2>
    friend
    detail::poly_handler_interface<R2, Args2...>*
    detail::get_impl(const poly_handler<R2(Args2...)> &h);

private:
    // TODO: use sbo optimization
    // https://github.com/ldionne/dyno/blob/34e68323d4ef5414bab3d039e4f6d7d278a96623/include/dyno/storage.hpp#L102
    detail::poly_handler_interface<R, Args...> *impl = NULL;
};

namespace detail {

template<class R, class... Args>
detail::poly_handler_interface<R, Args...>*
get_impl(const poly_handler<R(Args...)> &h)
{
    return h.impl;
}

} // namespace detail

} // namespace emilua

#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace boost {
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio {

template<class F>
struct associated_allocator<
    ::emilua::poly_handler<F>,
    std::allocator<void>
>
{
    using type = std::allocator<void>;

    static type get(const ::emilua::poly_handler<F>&,
                    const std::allocator<void>& = {}) noexcept
    {
        return {};
    }
};

template<class F, class Allocatee>
struct associated_allocator<
    ::emilua::poly_handler<F>,
    std::pmr::polymorphic_allocator<Allocatee>
>
{
    using type = std::pmr::polymorphic_allocator<Allocatee>;

    static type get(const ::emilua::poly_handler<F> &handler,
                    const type& a = type()) noexcept
    {
        auto r = handler.associated_resource();
        if (r)
            return type(r);
        else
            return a;
    }
};

template<class F, class Allocator>
struct associated_allocator<
    ::emilua::poly_handler<F>,
    Allocator
>
{
    class type
    {
    public:
        using value_type = typename Allocator::value_type;

        type(std::pmr::memory_resource* resource, const Allocator& a)
            : fallback_resource(a)
            , alloc(resource ? resource : &fallback_resource)
        {}

        value_type* allocate(std::size_t n)
        {
            return alloc.allocate(n);
        }

        void deallocate(value_type* p, std::size_t n)
        {
            return alloc.deallocate(p, n);
        }

    private:
        class fallback_resource_type : public std::pmr::memory_resource
        {
        public:
            fallback_resource_type(const Allocator& a)
                : alloc(a)
            {}

        private:
            void* do_allocate(std::size_t bytes, std::size_t alignment) override
            {
                assert(alignment == alignof(value_type));
                assert(bytes % sizeof(value_type) == 0);
                boost::ignore_unused(alignment);
                return alloc.allocate(bytes / sizeof(value_type));
            }

            void do_deallocate(
                void* p, std::size_t bytes, std::size_t alignment
            ) override
            {
                assert(alignment == alignof(value_type));
                assert(bytes % sizeof(value_type) == 0);
                boost::ignore_unused(alignment);
                return alloc.deallocate(reinterpret_cast<value_type*>(p),
                                        bytes / sizeof(value_type));
            }

            bool do_is_equal(
                const std::pmr::memory_resource&
            ) const noexcept override
            {
                return false;
            }

            Allocator alloc;
        } fallback_resource;

        std::pmr::polymorphic_allocator<typename Allocator::value_type> alloc;
    };

    static type get(const ::emilua::poly_handler<F> &handler,
                    const Allocator& a = Allocator()) noexcept
    {
        return type{handler.associated_resource(), a};
    }
};

template<class F, class Executor>
struct associated_executor<
    ::emilua::poly_handler<F>,
    Executor
>
{
    using type = asio::executor;

    static type get(
        const ::emilua::poly_handler<F> &h,
        const Executor &ex = Executor()
    )
    {
        auto impl = ::emilua::detail::get_impl(h);
        auto associated_ex = impl->get_executor();
        if (associated_ex)
            return associated_ex;

        return ex;
    }
};

} // namespace asio
#if !EMILUA_CONFIG_USE_STANDALONE_ASIO
} // namespace boost
#endif // !EMILUA_CONFIG_USE_STANDALONE_ASIO

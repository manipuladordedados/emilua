// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/core.hpp>

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/any_completion_handler.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/any_completion_handler.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua {

template<class Executor>
class basic_poly_stream
{
public:
    using lowest_layer_type = basic_poly_stream;
    using executor_type = Executor;

    template<class Executor1>
    struct rebind_executor
    {
        using other = basic_poly_stream<Executor1>;
    };

    lowest_layer_type& lowest_layer()
    {
        return *this;
    }

    const lowest_layer_type& lowest_layer() const
    {
        return *this;
    }

    virtual ~basic_poly_stream() = 0;

    virtual bool is_open() const = 0;

    // Free resources (e.g. file descriptors). One should always prefer
    // async_shutdown() instead. However it may be useful on terminal error
    // handling (e.g. Lua VM is dying).
    virtual void close() = 0;

    virtual executor_type get_executor() noexcept = 0;

    template<class CompletionToken>
    auto async_shutdown(CompletionToken&& token) {
        return asio::async_initiate<CompletionToken, void(asio_error_code)>(
            [](auto handler, basic_poly_stream* self) {
                self->async_shutdown_impl(std::move(handler));
            }, token, this);
    }

    template<class CompletionToken>
    auto async_read_some(asio::mutable_buffer buffer, CompletionToken&& token) {
        return asio::async_initiate<
            CompletionToken, void(asio_error_code, std::size_t)
        >([](
            auto handler, basic_poly_stream* self, asio::mutable_buffer buffer
        ) {
            self->async_read_some_impl(buffer, std::move(handler));
        }, token, this, buffer);
    }

    template<class CompletionToken>
    auto async_write_some(asio::const_buffer buffer, CompletionToken&& token) {
        return asio::async_initiate<
            CompletionToken, void(asio_error_code, std::size_t)
        >([](auto handler, basic_poly_stream* self, asio::const_buffer buffer) {
            self->async_write_some_impl(buffer, std::move(handler));
        }, token, this, buffer);
    }

private:
    virtual void async_shutdown_impl(
        asio::any_completion_handler<void(asio_error_code)>
    ) = 0;
    virtual void async_read_some_impl(
        asio::mutable_buffer buffer,
        asio::any_completion_handler<void(asio_error_code, std::size_t)> handler
    ) = 0;
    virtual void async_write_some_impl(
        asio::const_buffer buffer,
        asio::any_completion_handler<void(asio_error_code, std::size_t)> handler
    ) = 0;
};

template<class Executor>
inline basic_poly_stream<Executor>::~basic_poly_stream() = default;

template<class Executor>
class basic_poly_stream_ptr
    : private std::shared_ptr<basic_poly_stream<Executor>>
{
public:
    using lowest_layer_type = basic_poly_stream_ptr;
    using executor_type = Executor;

    template<class Executor1>
    struct rebind_executor
    {
        using other = basic_poly_stream_ptr<Executor1>;
    };

    lowest_layer_type& lowest_layer()
    {
        return *this;
    }

    const lowest_layer_type& lowest_layer() const
    {
        return *this;
    }

    basic_poly_stream<Executor>* get()
    {
        return static_cast<std::shared_ptr<basic_poly_stream<Executor>>&>(
            *this).get();
    }

    const basic_poly_stream<Executor>* get() const
    {
        return static_cast<const std::shared_ptr<basic_poly_stream<Executor>>&>(
            *this).get();
    }

    bool is_open() const
    {
        return get()->is_open();
    }

    void close()
    {
        return get()->close();
    }

    executor_type get_executor() noexcept
    {
        return get()->get_executor();
    }

    template<class CompletionToken>
    auto async_shutdown(CompletionToken&& token) {
        return get()->async_shutdown(std::forward<CompletionToken>(token));
    }

    template<class CompletionToken>
    auto async_read_some(asio::mutable_buffer buffer, CompletionToken&& token) {
        return get()->async_read_some(
            buffer, std::forward<CompletionToken>(token));
    }

    template<class CompletionToken>
    auto async_write_some(asio::const_buffer buffer, CompletionToken&& token) {
        return get()->async_write_some(
            buffer, std::forward<CompletionToken>(token));
    }
};

template<class Executor, class T>
basic_poly_stream_ptr<Executor> make_basic_poly_stream_ptr(T&& t)
{
    // TODO
}

using poly_stream = basic_poly_stream<asio::io_context::executor_type>;
using poly_stream_ptr = basic_poly_stream_ptr<asio::io_context::executor_type>;

} // namespace emilua

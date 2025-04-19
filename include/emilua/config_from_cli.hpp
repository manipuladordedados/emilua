// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#pragma once

#include <emilua/config.h>

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <asio/config.hpp>
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
#include <boost/asio/config.hpp>
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

namespace emilua {

#if EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = ::asio;
#else // EMILUA_CONFIG_USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif // EMILUA_CONFIG_USE_STANDALONE_ASIO

class config_from_cli : public asio::execution_context::service_maker
{
public:
    struct storage_type
    {
        template<class Archive>
        void serialize(Archive& ar)
        {
            ar(
                scheduler_concurrency_hint,
                scheduler_task_usec,
                scheduler_wait_usec,
                reactor_preallocated_io_objects,
                scheduler_locking,
                reactor_registration_locking,
                reactor_io_locking
            );
        }

#if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL < 2
        int scheduler_concurrency_hint = 1;
#else // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL < 2
        int scheduler_concurrency_hint = 0;
#endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL < 2

        int scheduler_task_usec = -1;
        int scheduler_wait_usec = -1;
        int reactor_preallocated_io_objects = 0;

#if EMILUA_CONFIG_THREAD_SUPPORT_LEVEL > 0
        bool scheduler_locking = true;
        bool reactor_registration_locking = true;
        bool reactor_io_locking = true;
#else // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL > 0
        bool scheduler_locking = false;
        bool reactor_registration_locking = false;
        bool reactor_io_locking = false;
#endif // EMILUA_CONFIG_THREAD_SUPPORT_LEVEL > 0
    } storage;

    void make(asio::execution_context& ctx) const override;

    void use_hint_unsafe_io()
    {
        // from
        // https://www.boost.org/doc/libs/1_88_0/doc/html/boost_asio/overview/core/configuration.html#boost_asio.overview.core.configuration.configuration_from_concurrency_hint
        storage.scheduler_concurrency_hint = 1;
        storage.scheduler_locking = true;
        storage.reactor_registration_locking = false;
        storage.reactor_io_locking = false;
    }

    void use_hint_unsafe()
    {
        // from
        // https://www.boost.org/doc/libs/1_88_0/doc/html/boost_asio/overview/core/configuration.html#boost_asio.overview.core.configuration.configuration_from_concurrency_hint
        storage.scheduler_concurrency_hint = 1;
        storage.scheduler_locking = false;
        storage.reactor_registration_locking = false;
        storage.reactor_io_locking = false;
    }

    template<class Archive>
    void serialize(Archive& ar)
    {
        ar(storage);
    }
};

} // namespace emilua

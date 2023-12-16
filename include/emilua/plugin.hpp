/* Copyright (c) 2021, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

#include <boost/preprocessor/list/cat.hpp>

// The plugin API/ABI is not stable. This trick adds a safety layer to avoid
// loading plugins compiled for the wrong version.
#define EMILUA_PLUGIN_SYMBOL BOOST_PP_LIST_CAT( ( \
        emilua_plugin_devel, ( \
            EMILUA_CONFIG_VERSION_MAJOR, ( \
                EMILUA_CONFIG_VERSION_MINOR, \
                (EMILUA_CONFIG_VERSION_PATCH, BOOST_PP_NIL) \
            ) \
        ) \
    ) )

namespace emilua {

// Once the plug-in is created, it is guaranteed that its functions will be
// called in this order:
//
// 1. init_appctx()
// 2. init_ioctx_services()
// 3. init_lua_module()
//
// init_lua_module() is only called on io contexts for which
// init_ioctx_services() has already been called (and the call succeeded).
class BOOST_SYMBOL_VISIBLE plugin
{
public:
    // Called only once per `appctx`. On normal conditions there is only one
    // `appctx` to the whole process, but the user might create custom
    // "launchers" to his project where multiple app contexts exist.
    //
    // A friendly reminder of a non-exhaustive list of things that are evil to
    // do before main() is called:
    //
    // * Spawning a thread (or process).
    // * Opening file descriptors.
    // * Buffering data into stdout/cout.
    // * Mutating (or even reading) environment variables.
    // * Calling any function that might do one of the previous things.
    //
    // If you need to do any of the above, please do not use constructors for
    // global variables to achieve it. Use init_appctx() instead. It might be
    // true that plugin loading happens after main() is called, but this
    // assumption will break on static builds that compile the plugin as part of
    // the same binary.
    virtual void init_appctx(
        const std::unique_lock<std::shared_mutex>& modules_cache_registry_wlock,
        app_context& appctx) noexcept;

    // It may be called multiple times on the same `ioctx` object. Use it to
    // register new services in the execution loop.
    virtual std::error_code init_ioctx_services(
        std::shared_lock<std::shared_mutex>& modules_cache_registry_rlock,
        asio::io_context& ioctx) noexcept;

    // Called only once per VM. It should push the Lua module (table) on the
    // stack on success and nothing on failure. It must only throw LUA_ERRMEM
    // errors and no other error (use the return value to notify other error
    // reasons).
    //
    // NOTE: If the function fails, it might be called again if Lua code try to
    // import the same module again.
    //
    // NOTE: There is no such a thing as an "async" native plugin load (e.g. a
    // plugin that suspends the caller module fiber). It'd be a hassle to
    // implement that. Just apply "lazy loading" techniques on the next layer if
    // you need such a thing (e.g. a function that checks if the module is ready
    // and suspend until so otherwise).
    virtual std::error_code init_lua_module(
        std::shared_lock<std::shared_mutex>& modules_cache_registry_rlock,
        vm_context& vm_ctx, lua_State* L);

    virtual ~plugin() = 0;
};

inline plugin::~plugin() = default;

} // namespace emilua

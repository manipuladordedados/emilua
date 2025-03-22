// Copyright (c) 2024, 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

#include <emilua/core.hpp>

namespace emilua {

#if BOOST_OS_WINDOWS

BOOST_SYMBOL_VISIBLE std::optional<std::string_view>
(*get_builtin_module)(const std::filesystem::path&) =
    [](const std::filesystem::path&) -> std::optional<std::string_view> {
    return std::nullopt;
};

BOOST_SYMBOL_VISIBLE std::optional<std::reference_wrapper<emilua::rdf_error_category>>
(*get_builtin_rdf_ec)(const std::filesystem::path&) =
    [](const std::filesystem::path&) ->
    std::optional<std::reference_wrapper<emilua::rdf_error_category>> {
    return std::nullopt;
};

BOOST_SYMBOL_VISIBLE std::optional<std::reference_wrapper<emilua::native_module>>
(*get_builtin_native_module)(std::string_view) =
    [](std::string_view) ->
    std::optional<std::reference_wrapper<emilua::native_module>> {
    return std::nullopt;
};

BOOST_SYMBOL_VISIBLE void (*create_native_modules)(
    const std::unique_lock<std::shared_mutex>&,
    app_context&) =
    [](const std::unique_lock<std::shared_mutex>&,
       app_context&) -> void {};

BOOST_SYMBOL_VISIBLE void (*destroy_native_modules)() = []() -> void {};

#else // BOOST_OS_WINDOWS

# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
BOOST_SYMBOL_VISIBLE
std::optional<std::string_view>
get_builtin_module(const std::filesystem::path&)
{
    return std::nullopt;
}

# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
BOOST_SYMBOL_VISIBLE
std::optional<std::reference_wrapper<emilua::rdf_error_category>>
get_builtin_rdf_ec(const std::filesystem::path&)
{
    return std::nullopt;
}

# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
BOOST_SYMBOL_VISIBLE
std::optional<std::reference_wrapper<emilua::native_module>>
get_builtin_native_module(std::string_view)
{
    return std::nullopt;
}

# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
BOOST_SYMBOL_VISIBLE
void create_native_modules(
    const std::unique_lock<std::shared_mutex>& /*modules_cache_registry_wlock*/,
    app_context& /*appctx*/)
{}

# if defined(EMILUA_STATIC_BUILD)
[[gnu::weak]]
# endif // defined(EMILUA_STATIC_BUILD)
BOOST_SYMBOL_VISIBLE
void destroy_native_modules()
{}

#endif // BOOST_OS_WINDOWS

} // namespace emilua

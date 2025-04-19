// Copyright (c) 2025 Vinícius dos Santos Oliveira
// SPDX-License-Identifier: MIT OR BSL-1.0

EMILUA_GPERF_DECLS_BEGIN(includes)
#include <emilua/config_from_cli.hpp>
EMILUA_GPERF_DECLS_END(includes)

namespace emilua {

class config_from_cli_service : public asio::config_service
{
public:
    explicit config_from_cli_service(
        asio::execution_context& ctx,
        const config_from_cli::storage_type& storage)
        : config_service{ctx}
        , storage{storage}
    {}

    const char* get_value(
        const char* section, const char* key, char* value,
        std::size_t value_len) const override;

private:
    config_from_cli::storage_type storage;
};

const char* config_from_cli_service::get_value(
    const char* section, const char* key, char* value,
    std::size_t value_len) const
{
    if (std::strcmp(section, "scheduler") == 0) {
        if (std::strcmp(key, "concurrency_hint") == 0) {
            std::snprintf(
                value, value_len, "%d", storage.scheduler_concurrency_hint);
            return value;
        } else if (std::strcmp(key, "task_usec") == 0) {
            std::snprintf(value, value_len, "%d", storage.scheduler_task_usec);
            return value;
        } else if (std::strcmp(key, "wait_usec") == 0) {
            std::snprintf(value, value_len, "%d", storage.scheduler_wait_usec);
            return value;
        } else if (std::strcmp(key, "locking") == 0) {
            return storage.scheduler_locking ? "1" : "0";
        }
    } else if (std::strcmp(section, "reactor") == 0) {
        if (std::strcmp(key, "preallocated_io_objects") == 0) {
            std::snprintf(
                value, value_len, "%d",
                storage.reactor_preallocated_io_objects);
            return value;
        } else if (std::strcmp(key, "io_locking") == 0) {
            return storage.reactor_io_locking ? "1" : "0";
        } else if (std::strcmp(key, "registration_locking") == 0) {
            return storage.reactor_registration_locking ? "1" : "0";
        }
    }
    return nullptr;
}

void config_from_cli::make(asio::execution_context& ctx) const
{
    asio::make_service<config_from_cli_service>(ctx, storage);
}

} // namespace emilua

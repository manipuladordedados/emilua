#include <unordered_map>
#include <system_error>
#include <algorithm>
#include <iostream>
#include <utility>
#include <string>
#include <vector>

#define X(A, B) { #A, generic_category().message(static_cast<int>(B)) }

using namespace std;

int main()
{
    vector<pair<string, string>> errors{
        X(EAFNOSUPPORT, errc::address_family_not_supported),
        X(EADDRINUSE, errc::address_in_use),
        X(EADDRNOTAVAIL, errc::address_not_available),
        X(EISCONN, errc::already_connected),
        X(E2BIG, errc::argument_list_too_long),
        X(EDOM, errc::argument_out_of_domain),
        X(EFAULT, errc::bad_address),
        X(EBADF, errc::bad_file_descriptor),
        X(EBADMSG, errc::bad_message),
        X(EPIPE, errc::broken_pipe),
        X(ECONNABORTED, errc::connection_aborted),
        X(EALREADY, errc::connection_already_in_progress),
        X(ECONNREFUSED, errc::connection_refused),
        X(ECONNRESET, errc::connection_reset),
        X(EXDEV, errc::cross_device_link),
        X(EDESTADDRREQ, errc::destination_address_required),
        X(EBUSY, errc::device_or_resource_busy),
        X(ENOTEMPTY, errc::directory_not_empty),
        X(ENOEXEC, errc::executable_format_error),
        X(EEXIST, errc::file_exists),
        X(EFBIG, errc::file_too_large),
        X(ENAMETOOLONG, errc::filename_too_long),
        X(ENOSYS, errc::function_not_supported),
        X(EHOSTUNREACH, errc::host_unreachable),
        X(EIDRM, errc::identifier_removed),
        X(EILSEQ, errc::illegal_byte_sequence),
        X(ENOTTY, errc::inappropriate_io_control_operation),
        X(EINTR, errc::interrupted),
        X(EINVAL, errc::invalid_argument),
        X(ESPIPE, errc::invalid_seek),
        X(EIO, errc::io_error),
        X(EISDIR, errc::is_a_directory),
        X(EMSGSIZE, errc::message_size),
        X(ENETDOWN, errc::network_down),
        X(ENETRESET, errc::network_reset),
        X(ENETUNREACH, errc::network_unreachable),
        X(ENOBUFS, errc::no_buffer_space),
        X(ECHILD, errc::no_child_process),
        X(ENOLINK, errc::no_link),
        X(ENOLCK, errc::no_lock_available),
        X(ENOMSG, errc::no_message),
        X(ENOPROTOOPT, errc::no_protocol_option),
        X(ENOSPC, errc::no_space_on_device),
        X(ENXIO, errc::no_such_device_or_address),
        X(ENODEV, errc::no_such_device),
        X(ENOENT, errc::no_such_file_or_directory),
        X(ESRCH, errc::no_such_process),
        X(ENOTDIR, errc::not_a_directory),
        X(ENOTSOCK, errc::not_a_socket),
        X(ENOTCONN, errc::not_connected),
        X(ENOMEM, errc::not_enough_memory),
        X(ENOTSUP, errc::not_supported),
        X(ECANCELED, errc::operation_canceled),
        X(EINPROGRESS, errc::operation_in_progress),
        X(EPERM, errc::operation_not_permitted),
        X(EOPNOTSUPP, errc::operation_not_supported),
        X(EWOULDBLOCK, errc::operation_would_block),
        X(EOWNERDEAD, errc::owner_dead),
        X(EACCES, errc::permission_denied),
        X(EPROTO, errc::protocol_error),
        X(EPROTONOSUPPORT, errc::protocol_not_supported),
        X(EROFS, errc::read_only_file_system),
        X(EDEADLK, errc::resource_deadlock_would_occur),
        X(EAGAIN, errc::resource_unavailable_try_again),
        X(ERANGE, errc::result_out_of_range),
        X(ENOTRECOVERABLE, errc::state_not_recoverable),
        X(ETXTBSY, errc::text_file_busy),
        X(ETIMEDOUT, errc::timed_out),
        X(ENFILE, errc::too_many_files_open_in_system),
        X(EMFILE, errc::too_many_files_open),
        X(EMLINK, errc::too_many_links),
        X(ELOOP, errc::too_many_symbolic_link_levels),
        X(EOVERFLOW, errc::value_too_large),
        X(EPROTOTYPE, errc::wrong_protocol_type),
    };

    // The program would be faster if we had just put these alias values
    // directly on the table above. However performance here is not
    // critical. This table appears separately as to document explicitly that
    // aliases are also taken care of.
    //
    // The meaning is "key may be an alias for value". For instance, if the
    // underlying OS doesn't have an explicit errno for EOPNOTSUPP (operation
    // not supported on socket), it'll reuse ENOTSUP (not supported). ENOTSUP is
    // more general than EOPNOTSUPP.
    std::unordered_map<std::string, std::string> aliases {
        { "EWOULDBLOCK", "EAGAIN" },
        { "EOPNOTSUPP", "ENOTSUP" }
    };

    for (auto& x : errors) {
        if (aliases.contains(x.first)) {
            x.first = aliases[x.first];
        }
    }

    // Some error messages might be a substring of other error
    // messages. Therefore we need to look for bigger error messages first when
    // we perform our sed expression.
    std::sort(errors.begin(), errors.end(), [](auto& a, auto& b) {
        return a.second.size() > b.second.size(); });

    for (auto& x : errors) {
        cout << x.first << ':' << x.second << endl;
    }
}

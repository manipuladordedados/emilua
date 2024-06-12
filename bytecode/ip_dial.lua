local get_address_info, socket_new, toendpoint, error, next, pcall, e_not_found = ...
return function(ep)
    local resolve_results = get_address_info(toendpoint(ep))
    local sock = socket_new()

    local last_error
    for _, v in next, resolve_results do
        if sock.is_open then sock:close() end
        local ok, e = pcall(function() sock:connect(v.address, v.port) end)
        if ok then
            return sock
        end
        last_error = e
    end
    if last_error == nil then
        last_error = e_not_found
    end
    error(last_error)
end

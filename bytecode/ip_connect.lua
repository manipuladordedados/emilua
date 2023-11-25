local error, type, next, pcall, e_not_found, e_invalid = ...
return function(sock, resolve_results, condition)
    local t_condition = type(condition)
    if
        type(resolve_results) ~= 'table' or
        (t_condition ~= 'nil' and t_condition ~= 'function')
    then
        error(e_invalid)
    end

    local last_error
    for _, v in next, resolve_results do
        if condition and not condition(last_error, v.address, v.port) then
            goto continue
        end

        if sock.is_open then sock:close() end
        local ok, e = pcall(function() sock:connect(v.address, v.port) end)
        if ok then
            return v.address, v.port
        end
        last_error = e

        ::continue::
    end
    if last_error == nil then
        last_error = e_not_found
    end
    error(last_error)
end

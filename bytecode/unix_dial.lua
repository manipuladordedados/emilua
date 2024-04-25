local socket_new, path_new, type, error, e_invalid = ...
return function(ep)
    if type(ep) ~= 'string' then
        error(e_invalid)
    end

    local sock = socket_new()
    sock:connect(path_new(ep))
    return sock
end

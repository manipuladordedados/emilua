local socket_new, path_new, type = ...
return function(p)
    if type(p) == 'string' then
        p = path_new(p)
    end

    local sock = socket_new()
    sock:connect(p)
    return sock
end

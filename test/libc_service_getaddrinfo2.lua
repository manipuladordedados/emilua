local libc_service = require 'libc_service'
local ip = require 'ip'

if _CONTEXT ~= 'main' then
    local results = ip.tcp.get_address_info('anonymous.invalid', 0)
    print(results[1].address, results[1].port)
    return
end

local master, slave = libc_service.new()

spawn_vm{
    module = '.',
    subprocess = {
        libc_service = slave,
        stdout = 'share',
        stderr = 'share',
    }
}

spawn(function() pcall(function()
    while true do
        master:receive()
        if master.function_ == 'getaddrinfo' then
            local node, service = master:arguments()
            if node == 'anonymous.invalid' then
                master:send({ip.address.new('::1%1'), 2})
            else
                master:use_slave_credentials()
            end
        else
            master:use_slave_credentials()
        end
    end
end) end):detach()

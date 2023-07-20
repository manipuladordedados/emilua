-- serialization/good
local NITER = 50000

local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local badinjector = require 'ipc_actor_badinjector'
local stream = require 'stream'
local system = require 'system'
local inbox = require 'inbox'

if _CONTEXT ~= 'main' then
    local host = inbox:receive()
    local NITER = inbox:receive()

    for i = 1, NITER do
        local ok, v = pcall(function()
            return inbox:receive()
        end)
        if not ok then
            -- bypass buffered IO
            stream.write_all(system.err, 'FAIL i=' .. i .. '\n')
            error(v)
        end
        if
            getmetatable(v) == 'file_descriptor' or
            getmetatable(v) == 'ipc_actor_channel'
        then
            v:close()
        elseif type(v) == 'table' then
            for _, l in pairs(v) do
                if
                    getmetatable(l) == 'file_descriptor' or
                    getmetatable(l) == 'ipc_actor_channel'
                then
                    l:close()
                end
            end
        end
    end

    host:send('OK')
else
    if system.environment.EMILUA_TEST_SEED then
        local seed = system.environment.EMILUA_TEST_SEED + 0
        badinjector.fuzzer_seed(seed)
        print('SEED=' .. seed)
    else
        print('SEED=' .. badinjector.fuzzer_seed())
    end

    if system.environment.EMILUA_TEST_FUZZER_NITER then
        NITER = system.environment.EMILUA_TEST_FUZZER_NITER + 0
    end

    if system.environment.EMILUA_TEST_FUZZER_SKIP then
        local n = system.environment.EMILUA_TEST_FUZZER_SKIP + 0
        for i = 1, n do
            badinjector.fuzzer_send_good()
        end
    end

    local my_channel = spawn_vm()
    my_channel:send(inbox)
    my_channel:send(NITER)

    for i = 1, NITER do
        local ok, e = pcall(function()
            badinjector.fuzzer_send_good(my_channel)
        end)
        if not ok then
            stream.write_all(system.err, 'HOST FAIL i=' .. i .. '\n')
            error(e)
        end
    end

    print(inbox:receive())
end

-- serialization/good
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm
local badinjector = require 'ipc_actor_badinjector'
local inbox = require 'inbox'

local function gen_oversized_table()
    local ret = {}
    for i = 1, badinjector.CONFIG_MESSAGE_MAX_MEMBERS_NUMBER do
        ret[i .. ''] = inbox
    end
    return ret
end

if _CONTEXT ~= 'main' then
    local ch = inbox:receive()['1']
    ch:send('hello')
else
    local my_channel = spawn_vm('ipc_actor_2_1')
    my_channel:send(gen_oversized_table())
    print(inbox:receive())
end

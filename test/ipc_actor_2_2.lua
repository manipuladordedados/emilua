-- serialization/bad
local badinjector = require 'ipc_actor_badinjector'

local function gen_oversized_table()
    local ret = {}
    for i = 1, badinjector.CONFIG_MESSAGE_MAX_MEMBERS_NUMBER + 1 do
        ret[i .. ''] = '.'
    end
    return ret
end

local my_channel = spawn_vm('', { subprocess = {} })
my_channel:send(gen_oversized_table())

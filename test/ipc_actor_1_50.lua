-- serialization/bad
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm

local function gen_oversized_str()
    local ret = ''
    for i = 1, 256 do
        ret = ret .. '.'
    end
    return ret
end

local my_channel = spawn_vm('')
my_channel:send{ value = gen_oversized_str() }

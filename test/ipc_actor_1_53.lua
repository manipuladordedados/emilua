-- serialization/bad
local function gen_oversized_table()
    local ret = {}
    for i = 1, 2000 do
        ret[i .. ''] = '.'
    end
    return ret
end

local my_channel = spawn_vm{ subprocess = {} }
my_channel:send(gen_oversized_table())

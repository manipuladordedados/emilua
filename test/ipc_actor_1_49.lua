-- serialization/bad
local function gen_oversized_str()
    local ret = ''
    for i = 1, 256 do
        ret = ret .. '.'
    end
    return ret
end

local my_channel = spawn_vm{ subprocess = {} }
my_channel:send(gen_oversized_str())

-- serialization/bad
local my_channel = spawn_vm{ subprocess = {} }
my_channel:send{ ['.'] = { ['.'] = '.' } }

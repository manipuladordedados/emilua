-- serialization/bad
local spawn_vm = require('./ipc_actor_libspawn').spawn_vm

local my_channel = spawn_vm('')
my_channel:send{ ['.'] = { ['.'] = '.' } }

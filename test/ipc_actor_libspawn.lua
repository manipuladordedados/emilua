local system = require 'system'
local fs = require 'filesystem'

local do_spawn_vm = spawn_vm

function spawn_vm(t)
    return do_spawn_vm(
        tostring(fs.path.new(system.environment.TESTS_PATH) / (t .. '.lua')),
        {
            subprocess = {
                stdout = 'share',
                stderr = 'share',
                environment = system.environment
            }
        }
    )
end

local system = require 'system'

local do_spawn_vm = spawn_vm

function spawn_vm(t)
    local mod = getfenv(2)._FILE
    return do_spawn_vm{
        module = mod,
        subprocess = {
            stdout = 'share',
            stderr = 'share',
            environment = system.environment
        }
    }
end

-- there's a bug in libpsx preventing it from work on C++ projects:
-- https://bugzilla.kernel.org/show_bug.cgi?id=218607

local fs = require 'filesystem'
local serial_port = require 'serial_port'
local stream = require 'stream'
local system = require 'system'
local inbox = require 'inbox'

local function print_no_new_privs()
    local status = stream.scanner.new{ field_separator = ':\t' }
    status.stream = serial_port.new()
    status.stream:assign(fs.open(fs.path.new('/proc/thread-self/status'), {'read_only'}))
    while true do
        local fields = status:get_line()
        if tostring(fields[1]) == 'NoNewPrivs' then
            print(fields[2])
            break
        end
    end
end

if _CONTEXT == 'main' then
    local vm2 = spawn_vm{
        module = '.',
        inherit_context = false,
        concurrency_hint = 1,
        new_master = true
    }
    vm2:send(inbox)
    inbox:receive()
    print_no_new_privs()
else
    system.set_no_new_privs()
    print_no_new_privs()
    local vm1 = inbox:receive()
    vm1:send(true)
end

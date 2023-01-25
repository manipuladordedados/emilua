-- serialization/bad
local spawn_vm = require('./linux_namespaces_libspawn').spawn_vm
local badinjector = require 'linux_namespaces_badinjector'
local sleep_for = require 'sleep_for'

local guest_code = [[
    local inbox = require 'inbox'
    print('RECEIVED:', inbox:receive())
]]

local my_channel = spawn_vm(guest_code)
badinjector.send_too_small(my_channel)
my_channel:close()
sleep_for(300) --< wait for some time before we kill the container

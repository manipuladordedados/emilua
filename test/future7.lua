local future = require 'future'

local p, f = future.new()
p = nil
collectgarbage('collect')
print(f:get())

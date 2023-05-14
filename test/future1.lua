local future = require 'future'

local p, f = future.new()
p:set_value(1)
print(f:get())

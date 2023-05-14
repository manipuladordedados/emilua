local future = require 'future'

local p, f = future.new()
p:set_value(nil)
print(f:get())

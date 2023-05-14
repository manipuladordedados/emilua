local future = require 'future'

local p, f = future.new()
p:set_error('thing')
print(f:get())

local future = require 'future'

local p, f = future.new()
print(f:get())
print('never gets here')

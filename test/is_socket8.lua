local unix = require 'unix'

local s = unix.stream.socket.new()
s:open()
s = s:release()
print(s:is_socket('unix', 'stream', 'tcp'))

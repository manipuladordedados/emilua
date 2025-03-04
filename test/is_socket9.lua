local unix = require 'unix'

local s = unix.datagram.socket.new()
s:open()
s = s:release()
print(s:is_socket('unix', 'datagram', 'udp'))

local unix = require 'unix'

local s = unix.stream.socket.new()
s:open()
s = s:release()
print(s:is_socket('unix'))
print(s:is_socket('inet'))
print(s:is_socket('inet6'))

print(s:is_socket('unix', 'stream'))
print(s:is_socket('unix', 'datagram'))
print(s:is_socket('unix', 'seqpacket'))

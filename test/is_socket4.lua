local ip = require 'ip'

local s = ip.tcp.socket.new()
s:open('v4')
s = s:release()
print(s:is_socket('unix'))
print(s:is_socket('inet'))
print(s:is_socket('inet6'))

print(s:is_socket('inet', 'stream'))
print(s:is_socket('inet', 'datagram'))
print(s:is_socket('inet', 'seqpacket'))

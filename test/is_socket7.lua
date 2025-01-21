local ip = require 'ip'

local s = ip.udp.socket.new()
s:open('v6')
s = s:release()
print(s:is_socket('unix'))
print(s:is_socket('inet'))
print(s:is_socket('inet6'))

print(s:is_socket('inet6', 'stream'))
print(s:is_socket('inet6', 'datagram'))
print(s:is_socket('inet6', 'seqpacket'))

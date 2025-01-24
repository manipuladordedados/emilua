local ip = require 'ip'

local s = ip.tcp.socket.new()
s:open('v4')
s = s:release()
print(s:is_socket('inet', 'stream', 'tcp'))
print(s:is_socket('inet', 'datagram', 'udp'))

local s = ip.udp.socket.new()
s:open('v4')
s = s:release()
print(s:is_socket('inet', 'stream', 'tcp'))
print(s:is_socket('inet', 'datagram', 'udp'))

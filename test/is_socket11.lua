local ip = require 'ip'

local s = ip.udp.socket.new()
s:open('v4')
s = s:release()
print(s:is_socket('inet', 'datagram', 'tcp'))

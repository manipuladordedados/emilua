local ip = require 'ip'

local s = ip.tcp.socket.new()
s:open('v4')
s = s:release()
print(s:is_socket('inet', 'stream', 'udp'))

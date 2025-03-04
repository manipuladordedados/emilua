-- capacity=0
local bs = byte_span.new(1, 2):sub(1, 2):sub(3)
print(#bs)
print(bs.capacity)

-- cannot have start beyond end
local bs = byte_span.new(1, 3):sub(3, 1)
print(#bs)
print(bs.capacity)

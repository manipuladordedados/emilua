local bs = byte_span.new(4)
bs[1] = 1
bs[2] = 2
bs[3] = 3
bs[4] = 4

bs = bs:first(4)
print(#bs, bs.capacity)
print(bs[1])
print(bs[2])
print(bs[3])
print(bs[4])

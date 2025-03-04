local bs = byte_span.new(4)
bs[1] = 1
bs[2] = 2
bs[3] = 3
bs[4] = 4

bs = bs:last(0)
print(#bs, bs.capacity)

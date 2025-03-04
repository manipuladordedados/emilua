local bs = byte_span.new(1, 5)
print(#bs)
print(bs.capacity)

local bs2 = bs:sub()
print(#bs2)
print(bs2.capacity)

bs2 = bs:sub(2)
print(#bs2)
print(bs2.capacity)

bs2 = bs2:sub(1, 4)
print(#bs2)
print(bs2.capacity)

bs2 = bs:sub(1, 5)
print(#bs2)
print(bs2.capacity)

bs2[1] = 20
print(bs2[1])
print(bs[1])

bs2[1] = 21
print(bs2[1])
print(bs[1])

bs2[2] = 22
print(bs2[2])
print(bs[1])

print(bs == bs2:sub(1, 1))
print(bs == bs2)
print(bs:sub(2) == bs2:sub(2, 1))
print(bs:sub(1, 2) == bs2:sub(2, 3))

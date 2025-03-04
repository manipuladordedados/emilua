local bs = byte_span.new(1, 3)
print(#bs)
print(bs.capacity)

bs:sub(1, 3):copy('foo')
print(bs)
print(bs:sub(1, 3))

print(bs:append('bar'))
print(bs:sub(1, 3))
print(bs:append('ba'))
print(bs:sub(1, 3))

bs = bs:append('oo')
print(bs:append(' ', bs, ' ', bs))

print(byte_span.append(nil, bs))

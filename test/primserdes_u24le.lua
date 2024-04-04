local bs = byte_span.new(3)

bs:set_u24le(0)
print(bs:get_u24le())
print(#bs, bs[1], bs[2], bs[3])

bs:set_u24le(256)
print(bs:get_u24le())
print(#bs, bs[1], bs[2], bs[3])

bs:set_u24le(257)
print(bs:get_u24le())
print(#bs, bs[1], bs[2], bs[3])

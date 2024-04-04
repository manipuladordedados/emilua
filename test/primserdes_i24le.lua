local bs = byte_span.new(3)

bs:set_i24le(-1)
print(bs:get_i24le())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24le(-2)
print(bs:get_i24le())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24le(0)
print(bs:get_i24le())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24le(256)
print(bs:get_i24le())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24le(257)
print(bs:get_i24le())
print(#bs, bs[1], bs[2], bs[3])

local bs = byte_span.new(2)

bs:set_u16le(0)
print(bs:get_u16le())
print(#bs, bs[1], bs[2])

bs:set_u16le(256)
print(bs:get_u16le())
print(#bs, bs[1], bs[2])

bs:set_u16le(257)
print(bs:get_u16le())
print(#bs, bs[1], bs[2])

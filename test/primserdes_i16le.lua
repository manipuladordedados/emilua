local bs = byte_span.new(2)

bs:set_i16le(-1)
print(bs:get_i16le())
print(#bs, bs[1], bs[2])

bs:set_i16le(-2)
print(bs:get_i16le())
print(#bs, bs[1], bs[2])

bs:set_i16le(0)
print(bs:get_i16le())
print(#bs, bs[1], bs[2])

bs:set_i16le(256)
print(bs:get_i16le())
print(#bs, bs[1], bs[2])

bs:set_i16le(257)
print(bs:get_i16le())
print(#bs, bs[1], bs[2])

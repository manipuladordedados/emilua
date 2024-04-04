local bs = byte_span.new(4)

bs:set_u32le(0)
print(bs:get_u32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_u32le(256)
print(bs:get_u32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_u32le(257)
print(bs:get_u32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

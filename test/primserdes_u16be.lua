local bs = byte_span.new(2)

bs:set_u16be(0)
print(bs:get_u16be())
print(#bs, bs[1], bs[2])

bs:set_u16be(256)
print(bs:get_u16be())
print(#bs, bs[1], bs[2])

bs:set_u16be(257)
print(bs:get_u16be())
print(#bs, bs[1], bs[2])

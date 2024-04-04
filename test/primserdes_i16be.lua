local bs = byte_span.new(2)

bs:set_i16be(-1)
print(bs:get_i16be())
print(#bs, bs[1], bs[2])

bs:set_i16be(-2)
print(bs:get_i16be())
print(#bs, bs[1], bs[2])

bs:set_i16be(0)
print(bs:get_i16be())
print(#bs, bs[1], bs[2])

bs:set_i16be(256)
print(bs:get_i16be())
print(#bs, bs[1], bs[2])

bs:set_i16be(257)
print(bs:get_i16be())
print(#bs, bs[1], bs[2])

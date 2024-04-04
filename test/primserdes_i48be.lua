local bs = byte_span.new(6)

bs:set_i48be(-1)
print(bs:get_i48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48be(-2)
print(bs:get_i48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48be(0)
print(bs:get_i48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48be(256)
print(bs:get_i48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48be(257)
print(bs:get_i48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

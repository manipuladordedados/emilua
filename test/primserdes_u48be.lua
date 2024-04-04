local bs = byte_span.new(6)

bs:set_u48be(0)
print(bs:get_u48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_u48be(256)
print(bs:get_u48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_u48be(257)
print(bs:get_u48be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

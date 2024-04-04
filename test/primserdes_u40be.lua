local bs = byte_span.new(5)

bs:set_u40be(0)
print(bs:get_u40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_u40be(256)
print(bs:get_u40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_u40be(257)
print(bs:get_u40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

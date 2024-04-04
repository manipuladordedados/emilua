local bs = byte_span.new(3)

bs:set_u24be(0)
print(bs:get_u24be())
print(#bs, bs[1], bs[2], bs[3])

bs:set_u24be(256)
print(bs:get_u24be())
print(#bs, bs[1], bs[2], bs[3])

bs:set_u24be(257)
print(bs:get_u24be())
print(#bs, bs[1], bs[2], bs[3])

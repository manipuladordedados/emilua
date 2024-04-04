local bs = byte_span.new(3)

bs:set_i24be(-1)
print(bs:get_i24be())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24be(-2)
print(bs:get_i24be())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24be(0)
print(bs:get_i24be())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24be(256)
print(bs:get_i24be())
print(#bs, bs[1], bs[2], bs[3])

bs:set_i24be(257)
print(bs:get_i24be())
print(#bs, bs[1], bs[2], bs[3])

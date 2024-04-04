local bs = byte_span.new(4)

bs:set_f32be(-1)
print(bs:get_f32be())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32be(-2)
print(bs:get_f32be())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32be(0)
print(bs:get_f32be())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32be(256)
print(bs:get_f32be())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32be(257)
print(bs:get_f32be())
print(#bs, bs[1], bs[2], bs[3], bs[4])

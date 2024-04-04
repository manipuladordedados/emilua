local bs = byte_span.new(4)

bs:set_f32le(-1)
print(bs:get_f32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32le(-2)
print(bs:get_f32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32le(0)
print(bs:get_f32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32le(256)
print(bs:get_f32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

bs:set_f32le(257)
print(bs:get_f32le())
print(#bs, bs[1], bs[2], bs[3], bs[4])

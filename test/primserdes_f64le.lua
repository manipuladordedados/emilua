local bs = byte_span.new(8)

bs:set_f64le(-1)
print(bs:get_f64le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64le(-2)
print(bs:get_f64le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64le(0)
print(bs:get_f64le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64le(256)
print(bs:get_f64le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64le(257)
print(bs:get_f64le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

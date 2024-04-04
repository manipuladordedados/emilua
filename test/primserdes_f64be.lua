local bs = byte_span.new(8)

bs:set_f64be(-1)
print(bs:get_f64be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64be(-2)
print(bs:get_f64be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64be(0)
print(bs:get_f64be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64be(256)
print(bs:get_f64be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

bs:set_f64be(257)
print(bs:get_f64be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6], bs[7], bs[8])

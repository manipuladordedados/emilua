local bs = byte_span.new(5)

bs:set_i40be(-1)
print(bs:get_i40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40be(-2)
print(bs:get_i40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40be(0)
print(bs:get_i40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40be(256)
print(bs:get_i40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40be(257)
print(bs:get_i40be())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

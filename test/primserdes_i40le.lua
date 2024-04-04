local bs = byte_span.new(5)

bs:set_i40le(-1)
print(bs:get_i40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40le(-2)
print(bs:get_i40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40le(0)
print(bs:get_i40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40le(256)
print(bs:get_i40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_i40le(257)
print(bs:get_i40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

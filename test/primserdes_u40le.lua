local bs = byte_span.new(5)

bs:set_u40le(0)
print(bs:get_u40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_u40le(256)
print(bs:get_u40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

bs:set_u40le(257)
print(bs:get_u40le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5])

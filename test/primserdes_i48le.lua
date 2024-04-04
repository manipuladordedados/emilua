local bs = byte_span.new(6)

bs:set_i48le(-1)
print(bs:get_i48le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48le(-2)
print(bs:get_i48le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48le(0)
print(bs:get_i48le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48le(256)
print(bs:get_i48le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

bs:set_i48le(257)
print(bs:get_i48le())
print(#bs, bs[1], bs[2], bs[3], bs[4], bs[5], bs[6])

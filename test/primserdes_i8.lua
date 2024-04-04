local bs = byte_span.new(1)

bs:set_i8(0)
print(bs:get_i8())
print(#bs, bs[1])

bs:set_i8(1)
print(bs:get_i8())
print(#bs, bs[1])

bs:set_i8(127)
print(bs:get_i8())
print(#bs, bs[1]);

bs:set_i8(-1)
print(bs:get_i8())
print(#bs, bs[1])

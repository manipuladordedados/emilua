local self, buffer, offset = ...
if not offset then
    offset = 1
end
self.buffer_ = buffer:first(buffer.capacity)
self.buffer_used = buffer:copy(buffer:sub(offset))
self.record_size = 0
self.record_terminator = nil

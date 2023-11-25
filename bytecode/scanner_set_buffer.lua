local self, buffer, offset = ...
if not offset then
    offset = 1
end
self.buffer_ = buffer:slice(1, buffer.capacity)
self.buffer_used = buffer:copy(buffer:slice(offset))
self.record_size = 0
self.record_terminator = nil

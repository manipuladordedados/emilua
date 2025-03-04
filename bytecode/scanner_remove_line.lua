local self = ...
local ready_wnd = self.buffer_:slice(1, self.buffer_used - self.record_size)
ready_wnd:copy(self.buffer_:slice(1 + self.record_size))
self.buffer_used = self.buffer_used - self.record_size
self.record_size = 0
self.record_terminator = nil

local self = ...

-- TODO: Golang doesn't use the buffer's head. Golang uses a moving window
-- over the buffer. Golang's strategy allows for less memory copies. Emilua
-- should do the same later down the road.
return self.buffer_:first(self.buffer_used), 1

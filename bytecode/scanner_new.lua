local mt, setmetatable, byte_span_new = ...

-- default sizes borrowed from Golang's bufio.Scanner
local INITIAL_BUFFER_SIZE = 4096
local MAX_RECORD_SIZE = 64 * 1024

return function(ret)
    if ret == nil then
        ret = {}
    end

    if not ret.record_separator then
        -- good default for network protocols
        ret.record_separator = '\r\n'
    end

    ret.buffer_ = byte_span_new(ret.buffer_size_hint or INITIAL_BUFFER_SIZE)
    ret.buffer_used = 0
    ret.record_size = 0
    ret.record_number = 0

    if not ret.max_record_size then
        ret.max_record_size = MAX_RECORD_SIZE
    end

    ret.buffer_size_hint = nil

    setmetatable(ret, mt)
    return ret
end

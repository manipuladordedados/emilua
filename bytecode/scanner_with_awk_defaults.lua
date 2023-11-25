local mt, setmetatable, byte_span_new, regex_new = ...

-- default sizes borrowed from Golang's bufio.Scanner
local INITIAL_BUFFER_SIZE = 4096
local MAX_RECORD_SIZE = 64 * 1024

local FS = regex_new{
    pattern = '[ \f\n\r\t\v]+',
    grammar = 'extended',
    nosubs = true,
    optimize = true
}

return function(stream)
    local ret = {
        trim_record = true,
        record_separator = '\n',
        field_separator = FS,

        buffer_ = byte_span_new(INITIAL_BUFFER_SIZE),
        buffer_used = 0,
        record_size = 0,
        record_number = 0,
        max_record_size = MAX_RECORD_SIZE,

        stream = stream
    }

    setmetatable(ret, mt)
    return ret
end

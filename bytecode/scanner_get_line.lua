local type, getmetatable, pcall, error, byte_span_new,
    regex_search, re_search_flags, regex_split,
    regex_patsplit, EEOF, EMSGSIZE = ...
return function(self)
    local ready_wnd = self.buffer_:slice(
        1, self.buffer_used - self.record_size)
    if self.record_size > 0 then
        ready_wnd:copy(self.buffer_:slice(1 + self.record_size))
        self.buffer_used = self.buffer_used - self.record_size
        self.record_size = 0
    end

    local record_separator = self.record_separator
    local record_separator_type = type(record_separator)
    local trim_record = self.trim_record
    local lws
    if type(trim_record) ~= 'boolean' then
        lws = trim_record
    end
    local field_separator = self.field_separator
    local field_separator_type = type(field_separator)
    if field_separator_type ~= 'string' then
        field_separator_type = getmetatable(field_separator)
    end
    local field_pattern = self.field_pattern
    local max_record_size = self.max_record_size
    local stream = self.stream
    local read_some = stream.read_some
    while true do
        local line
        if record_separator_type == 'string' then
            local idx = ready_wnd:find(record_separator)
            if idx then
                line = ready_wnd:slice(1, idx - 1)
                self.record_terminator = record_separator
                self.record_size = idx - 1 + #record_separator
            end
        else
            local m = regex_search(record_separator, ready_wnd,
                                   re_search_flags)
            if not m.empty then
                line = ready_wnd:slice(1, m[0].start - 1)
                self.record_terminator = ready_wnd:slice(
                    m[0].start, m[0].end_)
                self.record_size = m[0].end_
            end
        end
        if line then
            if trim_record then
                line = line:trimmed(lws)
            end
            self.record_number = self.record_number + 1
            if field_separator then
                if field_separator_type == 'string' then
                    local ret = {}
                    if #line == 0 then
                        return ret
                    end
                    local nf = 1
                    local idx = line:find(field_separator)
                    while idx do
                        ret[nf] = line:slice(1, idx - 1)
                        nf = nf + 1
                        -- TODO: use several indexes to avoid reslicing so
                        -- much
                        line = line:slice(idx + #field_separator)
                        idx = line:find(field_separator)
                    end
                    ret[nf] = line
                    return ret
                elseif field_separator_type == 'regex' then
                    return regex_split(field_separator, line)
                else
                    return field_separator(line)
                end
            elseif field_pattern then
                return regex_patsplit(field_pattern, line)
            else
                return line
            end
        end
        if #self.buffer_ == self.buffer_used then
            local new_size = #self.buffer_ * 2
            if new_size > max_record_size then
                new_size = max_record_size
            end
            if #self.buffer_ >= new_size then
                error(EMSGSIZE, 0)
            end
            local new_buffer = byte_span_new(new_size)
            new_buffer:copy(self.buffer_)
            self.buffer_ = new_buffer
        end
        local ok, nread = pcall(read_some, stream,
                                self.buffer_:slice(1 + self.buffer_used))
        if not ok then
            if nread ~= EEOF or #ready_wnd == 0 then
                error(nread, 0)
            end
            if trim_record then
                ready_wnd = ready_wnd:trimmed(lws)
            end
            if record_separator_type == 'string' then
                self.record_terminator = ''
            else
                self.record_terminator = byte_span_new(0)
            end
            self.record_size = #ready_wnd
            self.record_number = self.record_number + 1
            if field_separator then
                if field_separator_type == 'string' then
                    local ret = {}
                    if #ready_wnd == 0 then
                        return ret
                    end
                    local nf = 1
                    local idx = ready_wnd:find(field_separator)
                    while idx do
                        ret[nf] = ready_wnd:slice(1, idx - 1)
                        nf = nf + 1
                        -- TODO: use several indexes to avoid reslicing so
                        -- much
                        ready_wnd = ready_wnd:slice(idx + #field_separator)
                        idx = ready_wnd:find(field_separator)
                    end
                    ret[nf] = ready_wnd
                    return ret
                else
                    return regex_split(field_separator, ready_wnd)
                end
            elseif field_pattern then
                return regex_patsplit(field_pattern, ready_wnd)
            else
                return ready_wnd
            end
        end
        self.buffer_used = self.buffer_used + nread
        ready_wnd = self.buffer_:slice(1, self.buffer_used)
    end
end

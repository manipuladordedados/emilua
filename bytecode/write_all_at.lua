local type, byte_span_append = ...
return function(io_obj, offset, buffer)
   local ret = #buffer
   if type(buffer) == 'string' then
       buffer = byte_span_append(buffer)
   end
   while #buffer > 0 do
       local nwritten = io_obj:write_some_at(offset, buffer)
       offset = offset + nwritten
       buffer = buffer:slice(1 + nwritten)
   end
   return ret
end

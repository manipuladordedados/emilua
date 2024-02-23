local bit, S_ISUID, S_ISGID, S_ISVTX = ...
local bor = bit.bor
local lshift = bit.lshift

return function(user, group, other)
    if user == 'set_uid' then
        return S_ISUID
    elseif user == 'set_gid' then
        return S_ISGID
    elseif user == 'sticky_bit' then
        return S_ISVTX
    else
        return bor(lshift(user, 6), lshift(group, 3), other)
    end
end

local bit = ...
local bor = bit.bor
local lshift = bit.lshift

return function(user, group, other)
    return bor(lshift(user, 6), lshift(group, 3), other)
end

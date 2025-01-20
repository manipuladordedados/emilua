local inbox = require('inbox')

-- taken from http://snippets.luacode.org/snippets/Deep_Comparison_of_Two_Values_3
-- license unknown (we may have to remove this snippet later)
local function deepcompare(t1, t2, ignore_mt)
    local ty1 = type(t1)
    local ty2 = type(t2)
    if ty1 ~= ty2 then return false end
    -- non-table types can be directly compared
    if ty1 ~= 'table' and ty2 ~= 'table' then return t1 == t2 end
    -- as well as tables which have the metamethod __eq
    local mt = getmetatable(t1)
    if not ignore_mt and mt and mt.__eq then return t1 == t2 end
    for k1, v1 in pairs(t1) do
        local v2 = t2[k1]
        if v2 == nil or not deepcompare(v1, v2) then return false end
    end
    for k2, v2 in pairs(t2) do
        local v1 = t1[k2]
        if v1 == nil or not deepcompare(v1, v2) then return false end
    end
    return true
end

local body = {
    ignored = function() end,
    foo = {
        "foo1",
        {
            bar2 = "tsarstat",
            [ false ] = "whatever"
        },
        2
    },
    ignored2 = function() end,
    bar = {
        bar1b = "ienoenoin",
        [ true ] = "more whatever"
    },
    ignored3 = function() end,
    qux = {
    },
    ignored4 = function() end
}

if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    ch1:send{ from = ch2, body = body }
else assert(_CONTEXT == 'worker')
    local m = inbox:receive()

    if m.from then
        m.from:send{ body = m.body }
    else
        body.foo[2][false] = nil
        body.bar[true] = nil

        print(deepcompare(m.body.foo, body.foo))
        print(deepcompare(m.body.bar, body.bar))
        print(deepcompare(m.body.qux, body.qux))
        print(m.body.foo[3])
    end
end

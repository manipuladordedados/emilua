-- This test ensures join() propagates values back from the joinee.

local fib = spawn(function()
    print('secondary fiber starts')
    this_fiber.yield()
    return 30, 31, 32
end)

print('secondary fiber spawned')
this_fiber.yield()
local ret = {fib:join()}
print(#ret)
print(ret[1])
print(ret[2])
print(ret[3])
print('end of main fiber')

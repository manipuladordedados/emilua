-- This test ensures joiner's cancellator has been cleared by the join algorithm
-- and won't be reused again as an cancellator for another operation

local sleep = require('time').sleep

fib = spawn(function()
    spawn(function() end):join()
    spawn(function() fib:cancel() end):detach()
    this_fiber.disable_cancellation()
    spawn(function() sleep(0.01) print('foo') end):detach()
    sleep(0.02)
    print('bar')
end)

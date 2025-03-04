-- coroutine.resume() is an cancellation point, but only before executing the
-- argument
f = spawn(function()
    coroutine.wrap(function() end)()
    print('foo')
end)

f:cancel()

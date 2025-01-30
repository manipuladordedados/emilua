-- The cancel call is always valid

f = spawn(function() end)
f:cancel()
f:detach()
f:cancel()
f:cancel()

f = spawn(function() end)
f:cancel()
f:join()
f:cancel()
f:cancel()

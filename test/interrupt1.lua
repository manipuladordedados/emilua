-- Self-cancel
--
-- Do notice that Emilua doesn't expose vocabulary to express
-- pthread_cancel(pthread_self()). This is intentional.
--
-- If `fiber_canceled` is explicitly raised from the main fiber by user code, a
-- stack trace will be printed just like usual (but this logging behaviour might
-- change back and forth between versions as my mood changes).

fib = spawn(function()
    fib:cancel()
    print('foo')
    coroutine.wrap(function() this_fiber.yield() end)()
    print('bar')
end)

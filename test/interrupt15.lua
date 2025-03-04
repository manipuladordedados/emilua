-- Disable/restore cancellation

fib = spawn(function()
    fib:cancel()
    print('foo')
    this_fiber.disable_cancellation()
    this_fiber.yield()
    print('bar')
    this_fiber.restore_cancellation()
    -- Yes, 'baz' will still be printed. We DO have to wait until a new
    -- cancellation is reached before we can raise `fiber_canceled`. Consider
    -- the following: how would you implement an op whose some critical section
    -- cannot be canceled? Right, `disable_cancellation()`. But once you
    -- restore cancellation, the result might be ready already, and the proper
    -- behaviour is to deliver it to the user so it can do something with it. If
    -- `restore_cancellation()` itself was an cancellation point, this op
    -- function could not be implemented.
    print('baz')
    this_fiber.yield()
    print('qux')
end)

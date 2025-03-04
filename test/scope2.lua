-- scope() is not an cancellation point
--
-- This test is only here because pcall() is a cancellation point and scope()
-- is implemented in terms of pcall(). Some silly change could make scope()
-- inherit this undesired property. This test can prevent such break from going
-- unnoticed.
f = spawn(function()
    scope(function() end)
    print('foo')
end)

f:cancel()

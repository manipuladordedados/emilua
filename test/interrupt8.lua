-- Read invalid cancellation_caught on some fiber
fib = spawn(function()
    this_fiber.yield()
end)
this_fiber.yield()
print(fib.cancellation_caught)

-- Read invalid cancellation_caught on fiber who reacted to request
fib = spawn(function()
    this_fiber.yield()
end)
fib:cancel()
this_fiber.yield()
print(fib.cancellation_caught)

-- Join on fiber that hasn't reacted to an cancellation request yet
fib = spawn(function()
    this_fiber.yield()
end)
fib:cancel()
fib:join()
print(fib.cancellation_caught)

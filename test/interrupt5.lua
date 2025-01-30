-- Join on fiber that already reacted to an cancellation request
fib = spawn(function()
    this_fiber.yield()
end)
fib:cancel()
this_fiber.yield()
fib:join()
print(fib.cancellation_caught)

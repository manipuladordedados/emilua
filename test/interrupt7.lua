-- Read invalid cancellation_caught on not-yet-had-a-chance-to-run-fiber
fib = spawn(function() end)
print(fib.cancellation_caught)

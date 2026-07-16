// Recursive fib: stresses call/return, frame setup, and arithmetic dispatch.
fun fib(n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}

var start = clock();
var result = fib(30);
var elapsed = clock() - start;
print "fib(30) = " + str(result);
print "elapsed_s " + str(elapsed);

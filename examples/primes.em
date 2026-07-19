// Count primes below 10_000 by trial division.
// Run with: ./ember examples/primes.em
fun isPrime(n) {
  if (n < 2) return false;
  for (var d = 2; d * d <= n; d += 1) {
    if (n % d == 0) return false;
  }
  return true;
}

var start = clock();
var count = 0;
for (var i = 2; i < 10_000; i += 1) {
  if (isPrime(i)) count += 1;
}
print str(count) + " primes below 10000";
print "took " + str(clock() - start) + "s";

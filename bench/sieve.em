// Sieve of Eratosthenes: array-heavy workload (allocation, indexed reads and
// writes in tight loops). The JIT currently runs index ops through helpers,
// so this measures helper-call overhead against interpreter dispatch.
fun sieve(limit) {
  var composite = [];
  for (var i = 0; i <= limit; i += 1) {
    push(composite, false);
  }
  var count = 0;
  for (var p = 2; p <= limit; p += 1) {
    if (composite[p]) continue;
    count += 1;
    for (var multiple = p * p; multiple <= limit; multiple += p) {
      composite[multiple] = true;
    }
  }
  return count;
}

sieve(1000);  // warm-up
var start = clock();
var primes = sieve(200000);
var elapsed = clock() - start;
print "primes below 200000 = " + str(primes);
print "elapsed_s " + str(elapsed);

// Collatz: repeatedly halve evens and 3n+1 odds until reaching 1.
// Finds the longest chain for a starting point below 10_000.
// Run with: ./ember examples/collatz.em
fun chainLength(n) {
  var steps = 0;
  while (n != 1) {
    if (n % 2 == 0) n = n / 2;
    else n = 3 * n + 1;
    steps += 1;
  }
  return steps;
}

var start = clock();
var bestStart = 1;
var bestLength = 0;
for (var i = 1; i < 10_000; i += 1) {
  var length = chainLength(i);
  if (length > bestLength) {
    bestLength = length;
    bestStart = i;
  }
}
print str(bestStart) + " takes " + str(bestLength) + " steps, the longest below 10000";
print "took " + str(clock() - start) + "s";

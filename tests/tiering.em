// Exercises the interpreter->JIT tier boundary. With the default threshold
// (1000 calls) these functions tier up mid-loop; with EMBER_JIT=0 they never
// do; with EMBER_JIT_THRESHOLD=1 they tier immediately. Results must be
// identical in all three modes.

// Tier-up mid-workload: same function, both tiers contribute to the sum.
fun mix(n) {
  if (n % 2 == 0) return n / 2;
  return n * 3 + 1;
}
var sum = 0;
for (var i = 0; i < 1500; i += 1) {
  sum += mix(i);
}
print sum;              // expect: 1969125

// Mixed-tier composition: a hot loop allocating closures and calling them.
fun makeAdder(k) {
  fun add(x) { return x + k; }
  return add;
}
fun hotLoop() {
  var t = 0;
  for (var i = 0; i < 1200; i += 1) {
    var f = makeAdder(i);
    t += f(i);
  }
  return t;
}
print hotLoop();        // expect: 1438800

// A function that goes hot on numbers must still take its string slow path.
fun plus(a, b) { return a + b; }
var acc = 0;
for (var i = 0; i < 1100; i += 1) { acc = plus(acc, 1); }
print acc;              // expect: 1100
print plus("tier-", "safe");  // expect: tier-safe

// Tight numeric loop: stresses local variable access and dispatch. The loop
// lives in a function so it is eligible for tier-up (top-level script code
// never tiers).
fun run() {
  var sum = 0;
  for (var i = 0; i < 10000000; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}

run();  // warm-up call so the second call runs at full tier
var start = clock();
var sum = run();
var elapsed = clock() - start;
print "sum = " + str(sum);
print "elapsed_s " + str(elapsed);

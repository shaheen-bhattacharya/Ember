// Closure-heavy inner loop: stresses upvalue reads/writes and call overhead
// through a captured environment, the case tier-1 inlining should crush.
fun makeAccumulator() {
  var total = 0;
  fun add(n) {
    total = total + n;
    return total;
  }
  return add;
}

var start = clock();
var acc = makeAccumulator();
for (var i = 0; i < 2000000; i = i + 1) {
  acc(i);
}
var elapsed = clock() - start;
print "total = " + str(acc(0));
print "elapsed_s " + str(elapsed);

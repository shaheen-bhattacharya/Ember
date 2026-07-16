// Closures capture their environment: each counter gets its own state.
// Run with: ./ember examples/counters.em
fun makeCounter(start, step) {
  fun next() {
    start = start + step;
    return start;
  }
  return next;
}

var byOne = makeCounter(0, 1);
var byTen = makeCounter(100, 10);
print byOne();   // 1
print byOne();   // 2
print byTen();   // 110
print byOne();   // 3
print byTen();   // 120

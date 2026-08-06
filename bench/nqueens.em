// N-Queens solution counting: array reads in a hot inner loop plus deep
// call/return traffic — arrays and calls in the same workload, where sieve
// and fib each stress only one.
fun safe(cols, row, col) {
  for (var r = 0; r < row; r += 1) {
    var c = cols[r];
    if (c == col) return false;
    if (abs(c - col) == row - r) return false;
  }
  return true;
}

fun search(cols, row, n) {
  if (row == n) return 1;
  var count = 0;
  for (var col = 0; col < n; col += 1) {
    if (safe(cols, row, col)) {
      push(cols, col);
      count += search(cols, row + 1, n);
      pop(cols);
    }
  }
  return count;
}

var start = clock();
var result = search([], 0, 11);
var elapsed = clock() - start;
print "queens(11) = " + str(result);
print "elapsed_s " + str(elapsed);

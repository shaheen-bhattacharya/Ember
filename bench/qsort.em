// In-place quicksort over an LCG-generated array: array reads AND writes in
// the hot partition loop, plus recursive calls. The sieve benchmark is
// write-mostly and sequential; sorting adds data-dependent access patterns.
var seed = 42;
fun rnd() {
  seed = seed * 48271 % 2147483647;
  return seed;
}

fun partition(a, lo, hi) {
  var pivot = a[hi];
  var i = lo - 1;
  for (var j = lo; j < hi; j += 1) {
    if (a[j] < pivot) {
      i += 1;
      var tmp = a[i];
      a[i] = a[j];
      a[j] = tmp;
    }
  }
  var tmp = a[i + 1];
  a[i + 1] = a[hi];
  a[hi] = tmp;
  return i + 1;
}

fun quicksort(a, lo, hi) {
  if (lo >= hi) return;
  var p = partition(a, lo, hi);
  quicksort(a, lo, p - 1);
  quicksort(a, p + 1, hi);
}

fun isSorted(a) {
  for (var i = 1; i < len(a); i += 1) {
    if (a[i - 1] > a[i]) return false;
  }
  return true;
}

var n = 400_000;
var data = [];
for (var i = 0; i < n; i += 1) push(data, rnd());

var start = clock();
quicksort(data, 0, n - 1);
var elapsed = clock() - start;
print "qsort(" + str(n) + ") sorted = " + str(isSorted(data));
print "elapsed_s " + str(elapsed);

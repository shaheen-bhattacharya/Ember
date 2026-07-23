// In-place quicksort over an array, written in Ember.
// Run with: ./ember samples/sort.em

fun swap(a, i, j) {
  var tmp = a[i];
  a[i] = a[j];
  a[j] = tmp;
}

fun partition(a, lo, hi) {
  var pivot = a[hi];
  var i = lo;
  for (var j = lo; j < hi; j += 1) {
    if (a[j] < pivot) {
      swap(a, i, j);
      i += 1;
    }
  }
  swap(a, i, hi);
  return i;
}

fun quicksort(a, lo, hi) {
  if (lo >= hi) return nil;
  var p = partition(a, lo, hi);
  quicksort(a, lo, p - 1);
  quicksort(a, p + 1, hi);
  return nil;
}

fun isSorted(a) {
  for (var i = 1; i < len(a); i += 1) {
    if (a[i - 1] > a[i]) return false;
  }
  return true;
}

// Deterministic pseudo-random input (small LCG keeps every product exact
// in doubles).
fun makeInput(count) {
  var values = [];
  var seed = 42;
  for (var i = 0; i < count; i += 1) {
    seed = (seed * 75 + 74) % 65537;
    push(values, seed);
  }
  return values;
}

var numbers = makeInput(500);
print "count:  " + str(len(numbers));
print "sorted before: " + str(isSorted(numbers));
quicksort(numbers, 0, len(numbers) - 1);
print "sorted after:  " + str(isSorted(numbers));
print "min: " + str(numbers[0]);
print "max: " + str(numbers[len(numbers) - 1]);

// Negative indexes count from the end, Python-style.
var a = [10, 20, 30];
print a[-1];            // expect: 30
print a[-2];            // expect: 20
print a[-3];            // expect: 10
print a[len(a) - 1] == a[-1];  // expect: true

// Assignment too.
a[-1] = 99;
print a;                // expect: [10, 20, 99]
a[-3] = 1;
print a;                // expect: [1, 20, 99]
print a[-2] = 5;        // expect: 5

// Strings.
var s = "ember";
print s[-1];            // expect: r
print s[-5];            // expect: e
print s[-1] == s[len(s) - 1];  // expect: true

// Works in hot loops (the JIT shares the same index helpers).
fun lastSum(arr, n) {
  var total = 0;
  for (var i = 0; i < n; i += 1) {
    total += arr[-1] + arr[-2];
  }
  return total;
}
print lastSum([1, 2, 3, 4], 1000);  // expect: 7000

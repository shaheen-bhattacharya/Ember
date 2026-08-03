// Compound assignment on array elements: a[i] op= e.
var a = [10, 20, 30];
a[0] += 5;
print a[0];             // expect: 15
a[1] -= 20;
print a[1];             // expect: 0
a[2] *= 2;
print a[2];             // expect: 60
a[2] /= 4;
print a[2];             // expect: 15
a[2] %= 4;
print a;                // expect: [15, 0, 3]

// The result is an expression value, like plain assignment.
print a[0] += 1;        // expect: 16

// String concatenation through +=.
var words = ["ember", "lang"];
words[0] += "!";
print words[0];         // expect: ember!

// The array and index expressions are evaluated exactly once.
var calls = 0;
fun tracked(arr) {
  calls += 1;
  return arr;
}
var b = [1, 2];
tracked(b)[calls] += 10;   // calls becomes 1, then b[1] += 10
print b;                // expect: [1, 12]
print calls;            // expect: 1

// Nested targets: the inner index is a full expression.
var grid = [[1, 2], [3, 4]];
grid[1][0] += 100;
print grid[1][0];       // expect: 103
grid[0][grid[0][0]] *= 5;
print grid[0];          // expect: [1, 10]

// Hot-loop histogram, exercising the JIT path for the new opcode.
fun histogram(n) {
  var buckets = [0, 0, 0];
  for (var i = 0; i < n; i += 1) {
    buckets[i % 3] += 1;
  }
  return buckets;
}
print histogram(3000);  // expect: [1000, 1000, 1000]

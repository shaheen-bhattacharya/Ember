// break exits the innermost loop.
var i = 0;
while (true) {
  if (i == 3) break;
  i = i + 1;
}
print i;                // expect: 3

// break in a for loop.
var found = -1;
for (var j = 0; j < 100; j = j + 1) {
  if (j * j > 50) {
    found = j;
    break;
  }
}
print found;            // expect: 8

// break only exits the inner loop.
var log = "";
for (var a = 0; a < 3; a = a + 1) {
  for (var b = 0; b < 10; b = b + 1) {
    if (b == 2) break;
    log = log + str(a) + str(b);
  }
}
print log;              // expect: 000110112021

// Locals declared in the loop body are cleaned up on break.
while (true) {
  var local = "inside";
  break;
}
print "after";          // expect: after

// A function body inside a loop is its own break scope: the loop still runs.
var calls = 0;
for (var k = 0; k < 2; k = k + 1) {
  fun noop() { return nil; }
  noop();
  calls = calls + 1;
}
print calls;            // expect: 2

// continue skips to the next iteration.
var evens = "";
for (var i = 0; i < 8; i = i + 1) {
  if (i % 2 == 1) continue;
  evens = evens + str(i);
}
print evens;            // expect: 0246

// The for-loop increment still runs on continue (no infinite loop).
var count = 0;
for (var j = 0; j < 5; j = j + 1) {
  continue;
}
print "reached";        // expect: reached

// continue in a while loop jumps back to the condition.
var n = 0;
var sum = 0;
while (n < 10) {
  n = n + 1;
  if (n > 5) continue;
  sum = sum + n;
}
print sum;              // expect: 15

// continue only affects the innermost loop.
var log = "";
for (var a = 0; a < 2; a = a + 1) {
  for (var b = 0; b < 4; b = b + 1) {
    if (b % 2 == 0) continue;
    log = log + str(a) + str(b);
  }
}
print log;              // expect: 01031113

// Body locals are cleaned up on continue.
var k = 0;
while (k < 3) {
  k = k + 1;
  var local = "x" + str(k);
  if (k < 3) continue;
  print local;          // expect: x3
}

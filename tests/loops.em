// while and for loops.
var i = 0;
var sum = 0;
while (i < 5) {
  sum = sum + i;
  i = i + 1;
}
print sum;              // expect: 10

for (var j = 0; j < 3; j = j + 1) {
  print j;
}
// expect: 0
// expect: 1
// expect: 2

// for with no initializer clause.
var k = 10;
for (; k > 8; k = k - 1) {
  print k;
}
// expect: 10
// expect: 9

// Nested loops.
var count = 0;
for (var a = 0; a < 3; a = a + 1) {
  for (var b = 0; b < 3; b = b + 1) {
    count = count + 1;
  }
}
print count;            // expect: 9

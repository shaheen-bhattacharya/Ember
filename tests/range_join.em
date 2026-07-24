// range and join natives.
print range(4);                 // expect: [0, 1, 2, 3]
print range(2, 5);              // expect: [2, 3, 4]
print range(0);                 // expect: []
print range(3, 3);              // expect: []
print len(range(100));          // expect: 100

print join(range(3), "-");      // expect: 0-1-2
print join(["a", "b"], ", ");   // expect: a, b
print join([], "x") + "!";      // expect: !
print join([1, true, nil], "|");  // expect: 1|true|nil
print join(["solo"], ",");      // expect: solo

// Bad arguments answer nil.
print range("five");            // expect: nil
print join("not array", ",");   // expect: nil

// They compose with loops.
var total = 0;
var values = range(1, 11);
for (var i = 0; i < len(values); i += 1) {
  total += values[i];
}
print total;                    // expect: 55

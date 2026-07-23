// Array literals, indexing, mutation, and natives.
var a = [1, 2, 3];
print a;                // expect: [1, 2, 3]
print a[0];             // expect: 1
print a[2];             // expect: 3
print len(a);           // expect: 3

a[1] = 20;
print a;                // expect: [1, 20, 3]
print a[1] = 99;        // expect: 99

var empty = [];
print empty;            // expect: []
print len(empty);       // expect: 0

// Mixed types and nesting.
var mixed = [1, "two", true, nil, [3, 4]];
print mixed;            // expect: [1, two, true, nil, [3, 4]]
print mixed[4][1];      // expect: 4

// push and pop.
var stack = [];
push(stack, "a");
print push(stack, "b"); // expect: 2
print pop(stack);       // expect: b
print pop(stack);       // expect: a
print pop(stack);       // expect: nil

// Arrays are reference values; equality is identity.
var x = [1];
var y = x;
y[0] = 7;
print x[0];             // expect: 7
print x == y;           // expect: true
print [1] == [1];       // expect: false

// Expressions as elements and indices.
fun double(n) { return n * 2; }
var computed = [double(2), 1 + 2];
print computed[6 - 5];  // expect: 3

// String indexing.
var s = "ember";
print s[0];             // expect: e
print s[4];             // expect: r
print s[1] + s[2];      // expect: mb

// Self-referential arrays print with a depth cap instead of recursing.
var selfref = [1];
selfref[0] = selfref;
print selfref;          // expect: [[[[...]]]]

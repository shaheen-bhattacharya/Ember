// Conditional expressions.
print true ? "yes" : "no";        // expect: yes
print false ? "yes" : "no";       // expect: no
print 1 < 2 ? "lt" : "ge";        // expect: lt

// Only the taken branch evaluates.
fun boom() {
  print "boom";
  return 0;
}
print true ? 1 : boom();          // expect: 1
print false ? boom() : 2;         // expect: 2

// Right-associative chaining.
var n = 0;
print n == 0 ? "zero" : n > 0 ? "pos" : "neg";   // expect: zero
n = -5;
print n == 0 ? "zero" : n > 0 ? "pos" : "neg";   // expect: neg

// Nests in expressions, assignments, and arguments.
var abs5 = -5 < 0 ? 5 : -5;
print abs5;                       // expect: 5
print min(1 > 0 ? 10 : 20, 15);   // expect: 10
print [true ? 1 : 2, false ? 3 : 4];  // expect: [1, 4]

// Truthiness matches if: nil and false are falsey.
print nil ? "t" : "f";            // expect: f
print 0 ? "t" : "f";              // expect: t

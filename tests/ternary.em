// Conditional (ternary) expressions.
print true ? "yes" : "no";        // expect: yes
print false ? "yes" : "no";       // expect: no
print 1 < 2 ? "lt" : "ge";        // expect: lt

// Only the taken branch evaluates.
fun boom() {
  print "should not run";
  return 0;
}
print true ? 1 : boom();          // expect: 1
print false ? boom() : 2;         // expect: 2

// Right-associative: a ? b : c ? d : e is a ? b : (c ? d : e).
print false ? 1 : true ? 2 : 3;   // expect: 2
print false ? 1 : false ? 2 : 3;  // expect: 3

// Nesting in the then-branch needs no parentheses either.
print true ? true ? "tt" : "tf" : "f";  // expect: tt

// Binds looser than or/and and arithmetic.
print false or true ? "t" : "f"; // expect: t
print 1 + 1 == 2 ? 10 + 1 : 0;   // expect: 11

// Usable anywhere an expression is: initializers, arguments, returns.
var limit = len("abc") > 2 ? 100 : -100;
print limit;                      // expect: 100
print min(5, limit > 0 ? 3 : 7);  // expect: 3
fun clamp01(x) {
  return x < 0 ? 0 : x > 1 ? 1 : x;
}
print clamp01(-5);                // expect: 0
print clamp01(0.5);               // expect: 0.5
print clamp01(9);                 // expect: 1

// Truthiness matches if: nil and false are falsey, everything else truthy.
print nil ? "t" : "f";            // expect: f
print 0 ? "t" : "f";              // expect: t
print "" ? "t" : "f";             // expect: t

// Condition in a loop, exercising the JIT's jump handling when hot.
fun parity(n) {
  var odds = 0;
  for (var i = 0; i < n; i += 1) {
    odds += i % 2 == 1 ? 1 : 0;
  }
  return odds;
}
print parity(1000);               // expect: 500

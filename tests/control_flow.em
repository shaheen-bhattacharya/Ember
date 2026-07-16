// if/else and short-circuiting logical operators.
if (true) print "then";           // expect: then
if (false) print "bad"; else print "else";  // expect: else

var x = 5;
if (x > 3) {
  print "big";                    // expect: big
} else {
  print "small";
}

// and/or return the deciding operand, and short-circuit.
print true and "yes";             // expect: yes
print false and "never";          // expect: false
print false or "fallback";        // expect: fallback
print nil or 42;                  // expect: 42
print 1 == 1 and 2 == 2;          // expect: true

// Truthiness: nil and false are falsey, everything else is truthy.
if (0) print "zero is truthy";    // expect: zero is truthy
if (!nil) print "nil is falsey";  // expect: nil is falsey

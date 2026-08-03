// Number formatting (%.14g) and float edge cases.
print 0.1 + 0.2;        // expect: 0.3
print 1 / 3;            // expect: 0.33333333333333
print 1 / 0;            // expect: inf
print -1 / 0;           // expect: -inf
print 100000000;        // expect: 100000000
print 0.000001;         // expect: 1e-06
print -0.5;             // expect: -0.5
print 2.5 % 1;          // expect: 0.5
print 7 % -3;           // expect: 1
print 1.5 + 1.5;        // expect: 3

// Underscores are digit separators.
print 1_000_000;        // expect: 1000000
print 1_000 + 1;        // expect: 1001
print 3.141_59;         // expect: 3.14159
print 1_2_3;            // expect: 123

// Exponent literals.
print 1e3;              // expect: 1000
print 2.5e2;            // expect: 250
print 1e-2;             // expect: 0.01
print 1E+3;             // expect: 1000
print 5e0;              // expect: 5
print 1e3 == 1000;      // expect: true
print 1_000e1;          // expect: 10000
print 1e3 + 1e-3;       // expect: 1000.001

// An `e` with no digits after it is not an exponent: `e3` here is a
// variable, not part of the literal.
var e3 = 7;
print 1 + e3;           // expect: 8

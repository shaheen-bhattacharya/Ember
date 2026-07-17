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

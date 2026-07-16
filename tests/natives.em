// Native math functions.
print sqrt(16);                       // expect: 4
print sqrt(0);                        // expect: 0
print sqrt(2) > 1.414 and sqrt(2) < 1.415;  // expect: true
print abs(-5);                        // expect: 5
print abs(5);                         // expect: 5
print abs(-3.25);                     // expect: 3.25
print abs(0);                         // expect: 0

// Non-number arguments answer nil rather than erroring.
print sqrt("four");                   // expect: nil
print abs(nil);                       // expect: nil

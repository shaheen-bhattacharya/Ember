// Native math functions.
print sqrt(16);                       // expect: 4
print sqrt(0);                        // expect: 0
print sqrt(2) > 1.414 and sqrt(2) < 1.415;  // expect: true
print abs(-5);                        // expect: 5
print abs(5);                         // expect: 5
print abs(-3.25);                     // expect: 3.25
print abs(0);                         // expect: 0

print floor(3.7);                     // expect: 3
print floor(-3.7);                    // expect: -4
print ceil(3.2);                      // expect: 4
print ceil(-3.2);                     // expect: -3
print floor(5);                       // expect: 5

print round(2.4);                     // expect: 2
print round(2.5);                     // expect: 3
print round(-2.5);                    // expect: -3
print round(7);                       // expect: 7
print round("x");                     // expect: nil

print substr("hello", 0, 2);          // expect: he
print substr("hello", 1, 3);          // expect: ell
print substr("hello", 2, 100);        // expect: llo
print substr("hello", 9, 2) + "!";    // expect: !
print substr("hello", -1, 2);         // expect: nil
print substr(5, 0, 1);                // expect: nil

print len("hello");                   // expect: 5
print len("");                        // expect: 0
print len("a" + "bc");                // expect: 3

// Non-number arguments answer nil rather than erroring.
print sqrt("four");                   // expect: nil
print abs(nil);                       // expect: nil
print floor(true);                    // expect: nil
print ceil("up");                     // expect: nil
print len(42);                        // expect: nil

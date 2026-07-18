// IEEE 754 NaN semantics. These pin the current (correct) behavior so the
// planned NaN-boxing value representation can't silently change it: a boxed
// NaN must still compare unequal to itself.
var nan = 0 / 0;
print nan;              // expect: nan
print nan == nan;       // expect: false
print nan != nan;       // expect: true
print nan == 1;         // expect: false
print nan < 1;          // expect: false
print nan > 1;          // expect: false

// Infinities compare equal to themselves, unlike NaN.
print 1 / 0 == 1 / 0;   // expect: true
print -1 / 0 < 0;       // expect: true

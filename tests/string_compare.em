// Lexicographic (byte-order) string comparison.
print "apple" < "banana";   // expect: true
print "banana" < "apple";   // expect: false
print "abc" < "abd";        // expect: true
print "a" < "ab";           // expect: true
print "b" > "a";            // expect: true
print "same" <= "same";     // expect: true
print "same" >= "same";     // expect: true
print "Z" < "a";            // expect: true

// Number comparison is unchanged.
print 2 < 10;               // expect: true
print 10 >= 10;             // expect: true

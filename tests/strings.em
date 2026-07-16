// String literals, concatenation, and interned equality.
print "hello";                    // expect: hello
print "foo" + "bar";              // expect: foobar
print "a" + "b" + "c";            // expect: abc
print "same" == "same";           // expect: true
print "a" == "b";                 // expect: false
print "1" == 1;                   // expect: false

var greeting = "hello";
var name = "world";
print greeting + ", " + name + "!";  // expect: hello, world!

// Concatenation result equals an identical literal (interning at work).
print ("foo" + "bar") == "foobar";   // expect: true

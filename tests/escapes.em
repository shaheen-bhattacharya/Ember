// String escape sequences.
print "a\tb";           // expect: a	b
print "quote: \"hi\"";  // expect: quote: "hi"
print "back\\slash";    // expect: back\slash
print len("\n");        // expect: 1
print len("\\n");       // expect: 2
print "\"" == "\"";     // expect: true

// \n splits output across lines.
print "one\ntwo";
// expect: one
// expect: two

// Escapes and concatenation compose.
var tabbed = "x" + "\t" + "y";
print len(tabbed);      // expect: 3

// \xNN hex escapes: exactly two hex digits, any case.
print "\x41\x42\x43";   // expect: ABC
print "\x61";           // expect: a
print "\x4A" == "\x4a"; // expect: true
print len("\x00");      // expect: 1
print ord("\x7f");      // expect: 127
print "\x41" == chr(65); // expect: true

// Hex digits stop after two: the rest is literal text.
print "\x419";          // expect: A9


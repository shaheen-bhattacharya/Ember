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

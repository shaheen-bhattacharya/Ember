// split and indexOf natives.
print split("a,b,c", ",");        // expect: [a, b, c]
print split("one two", " ");      // expect: [one, two]
print split("nosep", ",");        // expect: [nosep]
print split("a--b", "--");        // expect: [a, b]
print len(split(",a,", ","));     // expect: 3
print split("", ",");             // expect: []

// split round-trips with join.
print join(split("x|y|z", "|"), "|");  // expect: x|y|z

// indexOf: position or -1.
print indexOf("ember", "be");     // expect: 2
print indexOf("ember", "e");      // expect: 0
print indexOf("ember", "z");      // expect: -1
print indexOf("aaa", "aa");       // expect: 0
print indexOf("abc", "");         // expect: 0

// Bad arguments answer nil.
print split(5, ",");              // expect: nil
print split("a", "");             // expect: nil
print indexOf("a", 5);            // expect: nil

// Composition: fields of a record.
var record = "name=ember;kind=vm;tier=1";
var fields = split(record, ";");
print fields[1];                  // expect: kind=vm
print split(fields[2], "=")[1];   // expect: 1

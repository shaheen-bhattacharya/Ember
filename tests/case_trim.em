// upper, lower, and trim natives.
print upper("hello");            // expect: HELLO
print lower("HELLO");            // expect: hello
print upper("MiXeD 123!");       // expect: MIXED 123!
print lower("MiXeD 123!");       // expect: mixed 123!
print upper("") + "!";           // expect: !
print lower(upper("round trip")); // expect: round trip

print trim("  spaced  ") + "!";  // expect: spaced!
print trim("\t\ntabs\r\n") + "!"; // expect: tabs!
print trim("no edges");          // expect: no edges
print trim("   ") + "!";         // expect: !
print trim("") + "!";            // expect: !
print len(trim("  ab "));        // expect: 2

// Case-insensitive comparison built from the primitives.
fun sameWord(a, b) {
  return lower(trim(a)) == lower(trim(b));
}
print sameWord(" Ember ", "ember");  // expect: true
print sameWord("Ember", "amber");    // expect: false

// Bad inputs answer nil.
print upper(5);                  // expect: nil
print lower(nil);                // expect: nil
print trim([1, 2]);              // expect: nil

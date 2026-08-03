// repeat native: string repetition.
print repeat("ab", 3);          // expect: ababab
print repeat("x", 5);           // expect: xxxxx
print repeat("hi ", 2) + "!";   // expect: hi hi !
print repeat("y", 1);           // expect: y
print repeat("y", 0) + "!";     // expect: !
print repeat("", 100) + "!";    // expect: !
print len(repeat("abc", 10));   // expect: 30

// Text layout: rules and indentation.
print repeat("-", 10);          // expect: ----------
fun indent(s, depth) { return repeat("  ", depth) + s; }
print indent("leaf", 2);        // expect:     leaf

// A right-align helper.
fun padLeft(s, width) {
  if (len(s) >= width) return s;
  return repeat(" ", width - len(s)) + s;
}
print padLeft("42", 5) + "|";   // expect:    42|
print padLeft("123456", 3) + "|";  // expect: 123456|

// Fractional, negative, or absurd counts answer nil.
print repeat("a", 1.5);         // expect: nil
print repeat("a", -1);          // expect: nil
print repeat("a", 100000000);   // expect: nil
print repeat(5, 2);             // expect: nil
print repeat("a", "b");         // expect: nil

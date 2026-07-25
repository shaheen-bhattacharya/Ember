// chr and ord natives.
print chr(65);          // expect: A
print chr(97);          // expect: a
print ord("A");         // expect: 65
print ord("abc");       // expect: 97
print chr(ord("z"));    // expect: z
print ord(chr(200));    // expect: 200

// Caesar shift built from the primitives.
fun shift(s, by) {
  var out = "";
  for (var i = 0; i < len(s); i += 1) {
    out += chr(ord(s[i]) + by);
  }
  return out;
}
print shift("hal", 1);  // expect: ibm

// Bad inputs answer nil.
print chr(-1);          // expect: nil
print chr(1.5);         // expect: nil
print chr("x");         // expect: nil
print ord(5);           // expect: nil
print ord("") == nil;   // expect: true

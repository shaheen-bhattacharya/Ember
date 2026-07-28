// num native: parse a string into a number (the inverse of str).
print num("42");             // expect: 42
print num("3.5");            // expect: 3.5
print num("-0.25");          // expect: -0.25
print num("1e3");            // expect: 1000
print num("1000000");        // expect: 1000000
print num("  7  ");          // expect: 7

// Numbers pass through unchanged.
print num(42);               // expect: 42
print num(-1.5);             // expect: -1.5

// Round-trips with str.
print num(str(123.25));      // expect: 123.25
print num(str(-8)) == -8;    // expect: true

// Parsing split fields.
var parts = split("3,14,159", ",");
var total = 0;
for (var i = 0; i < len(parts); i += 1) {
  total += num(parts[i]);
}
print total;                 // expect: 176

// Bad inputs answer nil.
print num("");               // expect: nil
print num("   ");            // expect: nil
print num("abc");            // expect: nil
print num("4x");             // expect: nil
print num("1.2.3");          // expect: nil
print num(true);             // expect: nil
print num(nil);              // expect: nil
print num([1]);              // expect: nil

// min and max natives.
print min(1, 2);        // expect: 1
print max(1, 2);        // expect: 2
print min(-1, -2);      // expect: -2
print max(-1, -2);      // expect: -1
print min(1.5, 1.5);    // expect: 1.5
print max(0, -0.5);     // expect: 0

// Variadic: any number of arguments, evaluated left to right.
print min(3, 1, 2);         // expect: 1
print max(3, 1, 2);         // expect: 3
print min(9, 8, 7, 6, 5);   // expect: 5
print max(-1, -2, -3, 0);   // expect: 0
print min(4);               // expect: 4
print max(4);               // expect: 4

// Clamp falls out of the pair.
fun clamp(x, lo, hi) { return max(lo, min(x, hi)); }
print clamp(15, 0, 10);     // expect: 10
print clamp(-5, 0, 10);     // expect: 0
print clamp(7, 0, 10);      // expect: 7

// Any non-number argument answers nil.
print max("a", "b");        // expect: nil
print min(1, 2, "x");       // expect: nil
print min() == nil;         // expect: true

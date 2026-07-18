// min and max natives.
print min(1, 2);        // expect: 1
print max(1, 2);        // expect: 2
print min(-1, -2);      // expect: -2
print max(-1, -2);      // expect: -1
print min(1.5, 1.5);    // expect: 1.5
print max(0, -0.5);     // expect: 0

// Wrong arity or non-number arguments answer nil.
print min(1);           // expect: nil
print max("a", "b");    // expect: nil

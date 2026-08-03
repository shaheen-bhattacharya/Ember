// pow, exp, and log natives.
print pow(2, 10);              // expect: 1024
print pow(9, 0.5);             // expect: 3
print pow(2, -1);              // expect: 0.5
print pow(7, 0);               // expect: 1
print pow(-2, 3);              // expect: -8

print exp(0);                  // expect: 1
print log(1);                  // expect: 0

// exp and log are inverses.
print log(exp(5));             // expect: 5
print exp(log(4)) > 3.999 and exp(log(4)) < 4.001;  // expect: true

// e itself, to a few digits.
print floor(exp(1) * 1000);    // expect: 2718

// log2 via the change-of-base identity.
fun log2(x) { return log(x) / log(2); }
print round(log2(1024));       // expect: 10

// pow composes with sqrt.
print pow(2, 0.5) == sqrt(2);  // expect: true

// Out-of-domain and bad inputs answer nil.
print log(0);                  // expect: nil
print log(-1);                 // expect: nil
print pow(2, "x");             // expect: nil
print pow("x", 2);             // expect: nil
print exp("x");                // expect: nil

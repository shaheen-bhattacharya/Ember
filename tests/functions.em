// Function declarations, calls, returns, recursion.
fun add(a, b) {
  return a + b;
}
print add(1, 2);        // expect: 3
print add;              // expect: <fn add>

fun noReturn() {
  var x = 1;
}
print noReturn();       // expect: nil

fun earlyReturn(n) {
  if (n < 0) return "negative";
  return "non-negative";
}
print earlyReturn(-5);  // expect: negative
print earlyReturn(5);   // expect: non-negative

fun fib(n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}
print fib(10);          // expect: 55
print fib(20);          // expect: 6765

fun isEven(n) {
  if (n == 0) return true;
  return isOdd(n - 1);
}
fun isOdd(n) {
  if (n == 0) return false;
  return isEven(n - 1);
}
print isEven(100);      // expect: true

// Native function is callable.
print clock() >= 0;     // expect: true

// Calling with the wrong number of arguments reports both counts and the
// frame the bad call happened in.
fun add(a, b) {
  return a + b;
}
fun caller() {
  return add(1);
}
caller();
// expect: Expected 2 arguments but got 1.
// expect: [line 7] in caller()
// expect: [line 9] in script

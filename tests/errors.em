fun explode(n) {
  return n * "boom";
}
explode(7);
// A runtime type error reports the message plus a stack trace, innermost
// frame first, with line numbers.
// expect: Operands must be numbers.
// expect: [line 2] in explode()
// expect: [line 4] in script

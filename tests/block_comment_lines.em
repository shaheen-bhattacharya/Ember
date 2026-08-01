// Line numbers survive multi-line block comments: the trace below must
// point at the real source lines, not drift by the comment's height.
/*
filler
filler
filler
*/
fun boom() {
  return 1 + nil;
}
boom();
// expect: Operands must be two numbers or two strings.
// expect: [line 9] in boom()
// expect: [line 11] in script

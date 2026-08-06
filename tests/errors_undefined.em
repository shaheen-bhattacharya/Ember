// Assigning to a name that was never declared is a runtime error, not an
// implicit global declaration — with the trace showing the guilty frame.
fun oops() {
  missing = 1;
}
oops();
// expect: Undefined variable 'missing'.
// expect: [line 4] in oops()
// expect: [line 6] in script

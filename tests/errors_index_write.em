// Strings are immutable: element assignment only works on arrays.
var s = "ember";
s[0] = "E";
// expect: Can only assign into arrays; got a string.
// expect: [line 3] in script

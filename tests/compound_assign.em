// Compound assignment desugars to read-modify-write.
var g = 10;
g += 5;
print g;                // expect: 15
g -= 3;
print g;                // expect: 12

// Works on locals.
{
  var x = 1;
  x += x;
  print x;              // expect: 2
}

// Works on strings via +'s concatenation.
var s = "ab";
s += "cd";
print s;                // expect: abcd

// Works through closures (upvalues).
fun makeCounter() {
  var n = 0;
  fun bump() {
    n += 1;
    return n;
  }
  return bump;
}
var c = makeCounter();
c();
print c();              // expect: 2

// The result is an expression value like plain assignment.
var y = 5;
print y += 5;           // expect: 10

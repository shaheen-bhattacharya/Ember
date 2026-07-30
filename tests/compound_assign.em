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

// Multiplicative forms.
var m = 6;
m *= 7;
print m;                // expect: 42
m /= 2;
print m;                // expect: 21
m %= 4;
print m;                // expect: 1

// The right side is a full expression: x *= a + b is x *= (a + b).
var z = 2;
z *= 3 + 4;
print z;                // expect: 14

// Multiplicative forms on locals and upvalues.
{
  var d = 1;
  fun doubler() { d *= 2; }
  doubler();
  doubler();
  print d;              // expect: 4
}

// Lexing: a /= b is compound assignment, a / = b is still an error; make
// sure /= doesn't swallow division or comments.
var q = 100;
q /= 10;  // trailing comment on a compound line
print q / 2;            // expect: 5

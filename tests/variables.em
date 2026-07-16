// Globals, locals, scoping, and shadowing.
var a = 1;
var b = 2;
print a + b;            // expect: 3

a = 10;
print a;                // expect: 10

{
  var a = 100;
  print a;              // expect: 100
  {
    var a = 1000;
    print a;            // expect: 1000
    b = a;
  }
  print a;              // expect: 100
}
print a;                // expect: 10
print b;                // expect: 1000

var uninitialized;
print uninitialized;    // expect: nil

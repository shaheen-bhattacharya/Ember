// Closures: capture, mutation through captures, sharing, and lifetimes.

// Each call to makeCounter gets its own captured `count`.
fun makeCounter() {
  var count = 0;
  fun increment() {
    count = count + 1;
    return count;
  }
  return increment;
}
var counter = makeCounter();
print counter();        // expect: 1
print counter();        // expect: 2
var counter2 = makeCounter();
print counter2();       // expect: 1
print counter();        // expect: 3

// Parameters are capturable locals too.
fun adder(x) {
  fun add(y) { return x + y; }
  return add;
}
print adder(3)(4);      // expect: 7

// Capture across more than one function boundary.
fun outer() {
  var x = "captured";
  fun middle() {
    fun inner() { return x; }
    return inner;
  }
  return middle;
}
print outer()()();      // expect: captured

// Two closures over the same variable share storage, even after the
// variable's block scope has exited and the upvalue has been closed.
var setValue;
var getValue;
{
  var shared = "before";
  fun set(v) { shared = v; }
  fun get() { return shared; }
  setValue = set;
  getValue = get;
}
print getValue();       // expect: before
setValue("after");
print getValue();       // expect: after

// A closed-over local survives the function returning.
fun makeGreeter(name) {
  var greeting = "hello, " + name;
  fun greet() { return greeting; }
  return greet;
}
var greet = makeGreeter("world");
print greet();          // expect: hello, world

// type native: the runtime kind of a value, as a string.
print type(nil);           // expect: nil
print type(true);          // expect: boolean
print type(false);         // expect: boolean
print type(0);             // expect: number
print type(1.5);           // expect: number
print type("hi");          // expect: string
print type("");            // expect: string
print type([1, 2]);        // expect: array
print type([]);            // expect: array

// All callables answer "function": declared functions, closures, natives.
fun f() {}
print type(f);             // expect: function
fun outer() {
  var captured = 1;
  fun inner() { return captured; }
  return inner;
}
print type(outer());       // expect: function
print type(clock);         // expect: function

// Expressions are typed by their result.
print type(1 + 1);         // expect: number
print type("a" + "b");     // expect: string
print type(1 < 2);         // expect: boolean
print type(pop([]));       // expect: nil

// Dispatch on type at runtime.
fun describe(v) {
  if (type(v) == "array") return "list of " + str(len(v));
  if (type(v) == "string") return "text " + v;
  return type(v);
}
print describe([1, 2, 3]); // expect: list of 3
print describe("go");      // expect: text go
print describe(7);         // expect: number

// type itself is a value.
print type(type);          // expect: function
print type(type(type));    // expect: string

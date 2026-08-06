// GC over array object graphs: cycles, deep nesting, and aliasing must all
// survive collections. Run with EMBER_GC_STRESS=1 (CI does) for the harshest
// version: every allocation below triggers a full collect.

// A cycle: two arrays pointing at each other. Marking must terminate.
var a = [1, nil];
var b = [2, a];
a[1] = b;

// Deep nesting: a 200-level list, built while garbage churns.
fun deepList(depth) {
  var head = [0, nil];
  var tail = head;
  for (var i = 1; i < depth; i += 1) {
    var node = [i, nil];
    var junk = "garbage-" + str(i);  // churn to force collections mid-build
    tail[1] = node;
    tail = node;
  }
  return head;
}
var list = deepList(200);

// More churn with the graph live.
fun churn(n) {
  for (var i = 0; i < n; i += 1) {
    var waste = [i, str(i), [i]];
  }
}
churn(50000);

// The cycle survived, and mutation through one alias shows through the other.
print a[0];             // expect: 1
print b[1][0];          // expect: 1
print a[1][0];          // expect: 2
a[0] = 100;
print b[1][0];          // expect: 100

// The deep list survived end to end.
fun listLength(head) {
  var n = 0;
  while (head != nil) {
    n += 1;
    head = head[1];
  }
  return n;
}
print listLength(list); // expect: 200
var last = list;
while (last[1] != nil) { last = last[1]; }
print last[0];          // expect: 199

// Interned strings referenced only by a surviving array stay alive.
var keeper = ["kept-" + str(12345)];
churn(10000);
print keeper[0];        // expect: kept-12345

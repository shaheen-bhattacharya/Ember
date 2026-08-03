// insert and removeAt natives: positional array editing.
var a = [1, 2, 4];
insert(a, 2, 3);
print a;                       // expect: [1, 2, 3, 4]
insert(a, 0, 0);
print a;                       // expect: [0, 1, 2, 3, 4]
insert(a, len(a), 5);          // inserting at len appends, like push
print a;                       // expect: [0, 1, 2, 3, 4, 5]
print insert(a, 6, 6)[6];      // expect: 6

print removeAt(a, 0);          // expect: 0
print removeAt(a, len(a) - 1); // expect: 6
print a;                       // expect: [1, 2, 3, 4, 5]
print removeAt(a, 2);          // expect: 3
print a;                       // expect: [1, 2, 4, 5]

// Insertion keeps a sorted array sorted: insertion sort in miniature.
fun insertSorted(xs, v) {
  var i = 0;
  while (i < len(xs) and xs[i] < v) i += 1;
  insert(xs, i, v);
}
var sorted = [];
insertSorted(sorted, 3);
insertSorted(sorted, 1);
insertSorted(sorted, 2);
print sorted;                  // expect: [1, 2, 3]

// Out-of-range and non-integer positions answer nil, untouched array.
var b = [7];
print insert(b, 2, 9);         // expect: nil
print insert(b, -1, 9);        // expect: nil
print insert(b, 0.5, 9);       // expect: nil
print removeAt(b, 1);          // expect: nil
print removeAt(b, -1);         // expect: nil
print removeAt([], 0);         // expect: nil
print b;                       // expect: [7]

// Bad inputs answer nil.
print insert("s", 0, 1);       // expect: nil
print removeAt(5, 0);          // expect: nil

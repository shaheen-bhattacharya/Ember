// sort native: in-place, homogeneous arrays only.
var nums = [3, 1, 2];
print sort(nums);       // expect: [1, 2, 3]
print nums;             // expect: [1, 2, 3]

print sort([5, -1, 3.5, 0]);       // expect: [-1, 0, 3.5, 5]
print sort(["pear", "apple", "fig"]);  // expect: [apple, fig, pear]
print sort([]);         // expect: []
print sort([7]);        // expect: [7]

// Sorted result chains into other natives.
print join(sort(["c", "a", "b"]), "");  // expect: abc

// Mixed or non-array input answers nil and leaves the array untouched.
var mixed = [1, "two"];
print sort(mixed);      // expect: nil
print mixed;            // expect: [1, two]
print sort("abc");      // expect: nil

// Duplicates and already-sorted input.
print sort([2, 1, 2, 1]);  // expect: [1, 1, 2, 2]
print sort([1, 2, 3]);     // expect: [1, 2, 3]

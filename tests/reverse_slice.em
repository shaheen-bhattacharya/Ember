// reverse: arrays flip in place (returned for chaining), strings copy.
var a = [1, 2, 3, 4];
reverse(a);
print a;                          // expect: [4, 3, 2, 1]
print reverse(a)[0];              // expect: 1
print reverse([]);                // expect: []
print reverse([7]);               // expect: [7]

print reverse("ember");           // expect: rebme
print reverse("") + "!";          // expect: !
var s = "keep";
print reverse(s);                 // expect: peek
print s;                          // expect: keep

// Palindrome check falls out.
fun isPalindrome(w) { return w == reverse(w); }
print isPalindrome("racecar");    // expect: true
print isPalindrome("ember");      // expect: false

// slice: copy of a sub-range, clamped at the end like substr.
var nums = range(0, 10);
print slice(nums, 0, 3);          // expect: [0, 1, 2]
print slice(nums, 7, 100);        // expect: [7, 8, 9]
print slice(nums, 4, 0);          // expect: []
print slice(nums, 42, 3);         // expect: []
print len(nums);                  // expect: 10

// The slice is a copy: mutating it leaves the source alone.
var head = slice(nums, 0, 2);
head[0] = 99;
print nums[0];                    // expect: 0
print head;                       // expect: [99, 1]

// sort a slice without disturbing the original (sort is in-place).
var vals = [3, 1, 2];
print sort(slice(vals, 0, 3));    // expect: [1, 2, 3]
print vals;                       // expect: [3, 1, 2]

// Bad inputs answer nil.
print reverse(5);                 // expect: nil
print slice("abc", 0, 1);         // expect: nil
print slice(nums, -1, 2);         // expect: nil
print slice(nums, 0, -1);         // expect: nil
